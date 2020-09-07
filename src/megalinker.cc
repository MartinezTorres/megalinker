////////////////////////////////////////////////////////////////////////
// Linker for MSX Megaroms
//
// Manuel Martinez (salutte@gmail.com)
//
// FLAGS: -std=c++14 -O0

#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <set>
#include <vector>
#include <functional>
#include <algorithm>

namespace { // MiniLog
	class Log {
		int level;
		std::stringstream * const sstr;
	public:
		static int &reportLevel() { static int rl = 0; return rl; };
		static int reportLevel(int l) { return reportLevel()=l; };
		Log (int level) : level(level), sstr(level>=reportLevel()?new std::stringstream():nullptr) {}
		~Log() {
			if (sstr) {
				if (level==0) std::cerr << "\x1b[34;1m";
				if (level==1 or level==-2) std::cerr << "\x1b[32;1m";
				if (level>=2 or level==-1) std::cerr << "\x1b[31;1m";
				std::cerr << "L" << level << " ";				

				std::cerr << sstr->str() << std::endl;
				std::cerr << "\x1b[0m";
				delete sstr;
			}
		}
		template<typename T> Log &operator<<(const T &v) { if (sstr) *sstr << v; return *this; }
		void operator<<(std::nullptr_t) {}
	};
}


struct AR { //ASSERT READ;
	
	std::string expected;
	AR(std::string expected) : expected(expected) {}
	
	friend std::istream & operator>>(std::istream &is, const AR& ar) {
		std::string read;
		is >> read;
		if (read != ar.expected) 
			throw std::runtime_error("Read: " + read + ", expected: " + ar.expected);
		return is;
	}
};

struct HEX { //READ HEXADECIMAL VALUE
	
	enum FORMAT {
		
		PLAIN, TWO_NIBBLES
	};
	
	struct HEX2DEC : std::vector<int> {
		HEX2DEC() {
			resize(256,0);
			for (int i='0'; i<='9'; i++)
				at(i) = i-'0';
				
			for (int i='A'; i<='F'; i++)
				at(i) = 10+ i-'A';

			for (int i='a'; i<='f'; i++)
				at(i) = 10+ i-'a';
		}
	};
	
	static uint32_t Hex2Dec(int v) {
		static HEX2DEC HD;
		return HD[v];
	}
	
	uint32_t &value;
	FORMAT format;
	
	HEX(uint32_t &value, FORMAT format) : value(value), format(format) {}
	
	friend std::istream &operator>>(std::istream &is, HEX&& hex) {
		
		hex.value = 0;
		
		if (hex.format==TWO_NIBBLES) {
			
			std::string s;
			if (not (is >> s)) return is;
			if (s.size()!=2) throw std::runtime_error("Not an hex byte");
			
			hex.value += Hex2Dec(s[0])<<4;
			hex.value += Hex2Dec(s[1])<<0;

		} else if (hex.format==PLAIN) {
			
			std::string s;
			if (not (is >> s)) throw std::runtime_error("Could not read expected value");

			for (auto &c : s)
				hex.value = hex.value * 16 + Hex2Dec(c);
		}
		return is;
	}
};

struct HEX2 : public HEX { HEX2(uint32_t &value) : HEX(value, HEX::TWO_NIBBLES) {} };

struct Module {
	
	struct Area {
		
		std::string name;
		uint32_t size;
		uint32_t addr;
		uint32_t rom_addr;
		enum { ABSOLUTE, RELATIVE} type;
		
	};

	struct Symbol {

		// Configuration Symbol
        const std::string prefix_configuration = "___ML_CONFIG_";
        bool isConfigurationSymbol() const { 
            
            if (name.substr(0,prefix_configuration.size()) != prefix_configuration) return false;
            return true;
        }

		// Module Segment Symbol
        const std::string prefix_segment = "___ML_SEGMENT_";
        bool isSegmentSymbol() const { 
            
            if (name.substr(0,prefix_segment.size()) != prefix_segment) return false;
            if (type == DEF) throw std::runtime_error("A program should not define a Megalinker Segment Symbol: " + name);
            
            if (name.size() < prefix_segment.size()+2) throw std::runtime_error("Short Megalinker Segment Symbol: " + name);
            if (name[prefix_segment.size()+1]!='_') throw std::runtime_error("Malformed Megalinker Segment Symbol: " + name);
            if (name[prefix_segment.size()] < 'A' or name[prefix_segment.size()] > 'D') throw std::runtime_error("Module Symbol: " + name + " requires a wrong page");
            return true;
        }

        std::string getSegmentName() const { 
			if (not isSegmentSymbol()) throw std::runtime_error("Megalinker Symbol: " + name + " is not a segment symbol");
			return name.substr(prefix_segment.size()+2); 
		}
		
        int getSegmentPage() const { 
			if (not isSegmentSymbol()) throw std::runtime_error("Megalinker Symbol: " + name + " is not a segment symbol");
			return name[prefix_segment.size()]-'A'; 
		}

		// Move Symbols Symbol
        const std::string prefix_move = "___ML_MOVE_SYMBOLS_TO_";
        bool isMoveSymbol() const { 
            
            if (name.substr( 0, prefix_move.size()) != prefix_move) return false;
            if (type == REF) throw std::runtime_error("A program should not refer to a Megalinker Segment Symbol: " + name);

			size_t pos = name.find("_FROM_");
			if (pos == std::string::npos) throw std::runtime_error("Move Symbol: " + name + " has no _FROM_ token");
			if (name.find("_FROM_",pos+1) != std::string::npos) throw std::runtime_error("Move Symbol: " + name + " has more than one _FROM_ tokens");
			
            return true;
        }

        std::string getMoveTarget() const { 

			if (not isMoveSymbol()) throw std::runtime_error("Module Symbol: " + name + " is not a module append symbol");						
			return name.substr(prefix_move.size(), name.find("_FROM_") - prefix_move.size()); 
        }

        std::string getMoveSource() const { 

			if (not isMoveSymbol()) throw std::runtime_error("Module Symbol: " + name + " is not a module append symbol");						
			return name.substr(name.find("_FROM_")+6);
        }
        
		std::string name;
		uint32_t addr;
		enum { DEF, REF} type;
		std::string areaName;
		
		uint32_t absoluteAddress;
	};
	
	std::string filename, name, content;
	std::vector<Area> areas;
	std::vector<Symbol> symbols;
	
	bool enabled = false;
	int page = -1;
	int segment = 0;
};

// preprocessModule makes a 1st pass scan through the REL file of a module.
// It determines the module name, its symbols, and its areas.
void preprocessModule(Module &module) {

	const std::set<std::string> known_areas = { 
		"_HEADER0",     // Fixed to segment 0, contains megarom initialization
		"_CODE",        // Banked code and const data
		"_DATA",        // Ram that does not need initialization
		"_XDATA",       // External Ram that does not need initialization
		"_GSINIT",      // Initialization code to be executed before calling main, sits in segment 0,
		"_GSFINAL",     // After code is initialized, it only remains to call main,
		"_INITIALIZED", // RAM that must be initialized
		"_INITIALIZER", // Contents to initialize RAM, sits in segment 0
		"_HOME"         // Non banked code that will be copied to RAM on initialization
	};
	
	std::istringstream isf(module.content);
	std::string line;

	module.name = "";
	// If the module comes from a rel file, the module name defaults to the filename.
	if (module.filename.find(".rel") != std::string::npos) {
		module.name = module.filename.substr(0, module.filename.find(".rel"));
		for (auto &&c : module.name) 
			if (c=='.') 
				c='_';
	}

	Log(2) << "File name: " << module.filename << " (" << module.name << ")"; 
		
	
	while (std::getline(isf, line)) {
		
		std::istringstream isl(line);
		std::string type;
		isl >> type;

		if (type=="XL2") { // HEADER
		} else if (type=="M") {
			
			// The module name is implicitly declared.
			isl >> module.name;
			Log(1) << "Module name: " << module.name << " (" << module.filename << ")"; 
			
		} else if (type=="O") { // NOT NEEDED
		} else if (type=="H") { // NOT NEEDED
		} else if (type=="S") {
			
			Module::Symbol symbol;
			std::string st = "   ";
			
			isl >> symbol.name >> st[0] >> st[1] >> st[2] >> HEX(symbol.addr, HEX::PLAIN);
			
				
			if (st=="Def") {
				symbol.type = Module::Symbol::DEF;
			} else if (st=="Ref") {
				symbol.type = Module::Symbol::REF;
			} else throw std::runtime_error("Symbol type unexpected");
			
			if (not module.areas.empty())
				symbol.areaName = module.areas.back().name;
			
			module.symbols.push_back(symbol);
			
			if (module.name.empty() and symbol.type == Module::Symbol::DEF and symbol.name.size()>1 and symbol.name[0]=='_') {
				module.name = symbol.name.substr(1);
				Log(1) << "Rel named after symbol" << module.name << " (" << module.filename << ")"; 
			}
			
		} else if (type=="A") {
			
			Module::Area area;
			
			uint32_t flags;
			isl >> area.name >> AR("size") >> HEX(area.size,HEX::PLAIN) 
				>> AR("flags") >> flags 
				>> AR("addr") >> HEX(area.addr,HEX::PLAIN);
				
			if (area.name.size()>0 and area.name[0]!='_')
				area.name = '_' + area.name;
							
			if (flags==0) {
				area.type = Module::Area::RELATIVE;
			} else if (flags==8) {
				area.type = Module::Area::ABSOLUTE;
			} else throw std::runtime_error("Unexpected flag");
			
			
			if (area.size>0) Log(1) << "Found area: " << area.name << " of size: " << area.size;
			if (area.size>0 and known_areas.count(area.name) == 0) throw std::runtime_error("Area " + area.name +" unknown");
			
			// NOTE: if the _HEADER0 area is defined, the module must be enabled. Other modules will be enabled on demand.
			if (area.name=="_HEADER0") module.enabled=true;

			module.areas.push_back(area);
			
		} else if (type=="T") { // NOT NOW
		} else if (type=="R") { // NOT NOW
		} else if (not type.empty()) {
			
			throw std::runtime_error("Unrecognized type: " + type);
		}
	}

	if (module.name.empty()) throw std::runtime_error("Module not given a name, and we could not determine a name for it: " + module.filename);
}


////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
	
	Log::reportLevel(3);
	
	std::string romName = "out.rom";
	std::map<std::string, std::vector<Module>> modules;
	
	// PREPROCESS ARGUMENTS AND INPUT FILES
	for (int i=1; i<argc; i++) {
		
		std::string arg = argv[i];

		if (arg[0] == '-') {
			
			if (arg == "-l") {
				
				if (i==argc-1) throw std::runtime_error("Log level required but not specified");
				i++;

				int level;
				if (sscanf(argv[i], "%i", &level) != 1) throw std::runtime_error("Unrecognized level" + arg);
				Log::reportLevel(level);
				
				
			} else throw std::runtime_error("Unknown flag " + arg);
			
			
		} else if (arg.substr(arg.find_last_of(".")) == ".rom") {

			Log(1) << "Rom name: " << arg;
			romName = arg;
			
		} else if (arg.substr(arg.find_last_of(".")) == ".rel") {	
			
			Log(1) << "Processing: " << arg;
			Module module;
			module.filename = arg;
			
			std::ifstream isf(arg);
			std::stringstream buffer;
			buffer << isf.rdbuf();
			module.content = buffer.str();
			
			preprocessModule(module);
			if (modules.count(module.name)) throw std::runtime_error("File " + arg + " declares a module already defined in: " + modules[module.name].front().filename);
			modules[module.name].push_back(module);

		} else if (arg.substr(arg.find_last_of(".")) == ".lib") {	

			Log(1) << "Processing: " << arg;
			std::ifstream isf(arg);
			
			std::string ar_signature = "!<arch>\n"; 
			isf.read(&ar_signature[0],8); 
			if (ar_signature != "!<arch>\n") throw std::runtime_error("Wrong signature in archive: " + arg);
			
			while (isf) {
				std::string ar_file_name(16+1,0);
				isf.read(&ar_file_name[0],16); 
				
				if (!isf) break;

				std::string ar_buffer(12+6+6+8+1,0);
				isf.read(&ar_buffer[0],12+6+6+8); 

				std::string ar_size(10+1,0);
				isf.read(&ar_size[0],10); 
				std::istringstream issize(ar_size);
				size_t ar_file_size;
				issize >> ar_file_size;
				
				isf.read(&ar_buffer[0],2); 

				Log(1) << "Found in archive: " << ar_file_name << "(" << ar_file_size << ")";

				if (!isf) throw std::runtime_error("library terminates before reading full file");
				
				Module module;
				module.filename = ar_file_name;
				module.content.resize(ar_file_size);
				isf.read(&module.content[0],ar_file_size); 

				if (!isf) break;
				if (!isf) throw std::runtime_error("library terminates before reading entire file");
				
				
				if (module.content.size()>10 and module.content.substr(0,3)=="XL2") {
					
					preprocessModule(module);
					if (modules.count(module.name)) throw std::runtime_error("File " + arg + " declares a module already defined in: " + modules[module.name].front().filename);
					modules[module.name].push_back(module);

				} else {
					
					Log(2) << "File " << ar_file_name << " not a relocatable object file";
				} 
				
				if (ar_file_size % 2 == 1) isf.get(); // Align to 2
			}
		}
	}
			
	// PROCESS THE MOVE_TO_ DIRECTIVE
	{
		std::map<std::string, std::string > moveDirectives;

		for (auto &mp : modules) {
			for (auto &module : mp.second) {
				for (auto &sym : module.symbols) {
					if (not sym.isMoveSymbol()) continue;
					std::string source = sym.getMoveSource();
					std::string target = sym.getMoveTarget();
					
					if (moveDirectives.count(source) and moveDirectives[source] != target) throw std::runtime_error("Module symbols can not be send to more than one target: (" + source + " -> " + target + ")" );
					if (modules.count(source)==0) throw std::runtime_error("Unknown source module: " + source );
					
					moveDirectives[source] = target;
				}
			}
		}
		
		for (auto &md : moveDirectives) {

			if (moveDirectives.count(md.second)) 
				throw std::runtime_error("Moving symbols functionality does not support chains (yet)" );
			
			for (auto &mp : modules[md.first]) {
				Log(3) << "Moving module: " << mp.name << " to " << md.second;
				modules[md.second].push_back(mp);
			}
			modules.erase(md.first);
		}
	}
	
	// ENABLE ALL REQUIRED FILES / MODULES
	for (;;) {
		
		bool updated = false;

		std::map<std::string,int> referencedSymbols;

		for (auto &mp : modules) {
			for (auto &module : mp.second) {
				
				if (not module.enabled) continue;
				
				for (auto &sym : module.symbols) {
					
					if (sym.type != Module::Symbol::REF) continue;
					
					if (sym.isConfigurationSymbol()) continue;
					
					if (sym.isSegmentSymbol()) {
						
						std::string requiredModule = sym.getSegmentName(); 
						
						if (modules.count(requiredModule)==0) throw std::runtime_error("Module: " + module.name + " requires unknown module: " + requiredModule );

						continue;
					}
					
					referencedSymbols[sym.name] = 0;
				}
			}
		}

		for (auto &mp : modules) {
			for (auto &module : mp.second) {
				for (auto &sym : module.symbols) {
					
					if (sym.type != Module::Symbol::DEF) continue;

					if (sym.isConfigurationSymbol()) continue;
						
					if (not module.enabled and referencedSymbols.count(sym.name)) {
						module.enabled = true;
						updated = true;
					}
					
					if (module.enabled and referencedSymbols.count(sym.name)) {
						
						if (referencedSymbols[sym.name]>0) throw std::runtime_error("Symbol: " + sym.name + " defined multiple times");
						referencedSymbols[sym.name]++;
					}
				}
			}
		}
		
		for (auto &ref : referencedSymbols)
			if (ref.second==0)
				throw std::runtime_error("Referenced Symbol: " + ref.first + " not defined");
				
		if (not updated) break;
	}

	// REMOVE NON ENABLED SUB-MODULES
	for (auto &mp : modules) {
		auto &m = mp.second;
		auto it =  std::remove_if(m.begin(), m.end(), [](const Module &item) { return not item.enabled; });
		m.erase(it, m.end());
	}

	// REMOVE MODULES WITHOUT ACTIVE SUB-MODULES
	for (auto it = modules.begin(); it != modules.end(); ) {
        if (it->second.empty())
			it = modules.erase(it);
        else
            ++it;
    }
 
	// PAGE ALLOCATION AND ERROR CHECKING
	{
		for (auto &mp : modules) {
			for (auto &module : mp.second) {
				for (auto &sym : module.symbols) {
					if (not sym.isSegmentSymbol()) continue;
					
					std::string requiredModule = sym.getSegmentName(); 
					int requiredPage = sym.getSegmentPage();
					
					for (auto &m : modules[requiredModule]) {
						if (m.page == -1)
							m.page = requiredPage;
					
						if (m.page != requiredPage)
							throw std::runtime_error("Module " + requiredModule + " required at different pages");
					}
				}
			}
		}
	}
	
	std::map<std::string, uint32_t> megalinkerSymbols;
	// FIND ALL MEGALINKER DEFINED CONFIGURATION SYMBOLS
	{
		for (auto &mp : modules) {
			for (auto &module : mp.second) {
				for (auto &sym : module.symbols) {
					if (sym.type != Module::Symbol::DEF) continue;
					if (not sym.isConfigurationSymbol()) continue;
										
					if (megalinkerSymbols.count(sym.name) and megalinkerSymbols[sym.name] != sym.addr)
						throw std::runtime_error("Conflicting definitions of: " + sym.name );
						
					megalinkerSymbols[sym.name] = sym.addr;
				}
			}
		}
	}
	
	uint32_t rom_ptr = -1;
	uint32_t ram_ptr = -1;
	if (megalinkerSymbols.count("___ML_CONFIG_RAM_START")==0) throw std::runtime_error("___ML_CONFIG_RAM_START not defined");
	ram_ptr = megalinkerSymbols["___ML_CONFIG_RAM_START"];
	
	// ALLOCATE ALL NON BANKABLE AREAS
	{

		for (auto &mp : modules) {
			for (auto &module : mp.second) {
				for (auto &area:  module.areas) {
					if (area.name!="_HEADER0") continue;
					if (rom_ptr!=uint32_t(-1)) throw std::runtime_error(area.name + " defined more than once: " + module.filename);
					if (area.type != Module::Area::ABSOLUTE) throw std::runtime_error(area.name + " not absolute: " + module.filename);
					if (area.addr != 0x4000) throw std::runtime_error("HEADER not at 0x4000: " + module.filename);
		
					rom_ptr = area.addr;
					area.rom_addr = area.addr;
					rom_ptr += area.size;
				}
			}
		}
		
		for (auto &mp : modules) {
			for (auto &module : mp.second) {
				for (auto &area:  module.areas) {
					if (area.name!="_GSINIT") continue;
					if (area.type != Module::Area::RELATIVE) throw std::runtime_error(area.name + " not relative: " + module.filename);

					area.addr = rom_ptr;
					area.rom_addr = area.addr;
					rom_ptr += area.size;
				}
			}
		}

		for (auto &mp : modules) {
			for (auto &module : mp.second) {
				for (auto &area:  module.areas) {
					if (area.name!="_GSFINAL") continue;
					if (area.type != Module::Area::RELATIVE) throw std::runtime_error(area.name + " not relative: " + module.filename);

					area.addr = rom_ptr;
					area.rom_addr = area.addr;
					rom_ptr += area.size;
				}
			}
		}

		megalinkerSymbols["___ML_CONFIG_INIT_ROM_START"] = rom_ptr;
		megalinkerSymbols["___ML_CONFIG_INIT_RAM_START"] = ram_ptr;

		for (auto &mp : modules) {
			for (auto &module : mp.second) {
				for (auto &area:  module.areas) {
					if (area.name!="_HOME") continue;
					if (area.type != Module::Area::RELATIVE) throw std::runtime_error(area.name + " not relative: " + module.filename);

					area.addr = ram_ptr;
					area.rom_addr = rom_ptr;
					rom_ptr += area.size;
					ram_ptr += area.size;
				}
			}
		}


		for (auto &mp : modules) {
			for (auto &module : mp.second) {
				for (auto &area:  module.areas) {
					if (area.name!="_INITIALIZER") continue;
					if (area.type != Module::Area::RELATIVE) throw std::runtime_error(area.name + " not relative: " + module.filename);

					area.addr = rom_ptr;
					area.rom_addr = area.addr;
					rom_ptr += area.size;
				}
			}
		}
		megalinkerSymbols["___ML_CONFIG_INIT_SIZE"] = rom_ptr - megalinkerSymbols["___ML_CONFIG_INIT_ROM_START"];

		for (auto &mp : modules) {
			for (auto &module : mp.second) {
				for (auto &area:  module.areas) {
					if (area.name!="_INITIALIZED") continue;
					if (area.type != Module::Area::RELATIVE) throw std::runtime_error(area.name + " not relative: " + module.filename);

					area.addr = ram_ptr;
					area.rom_addr = uint32_t(-1);
					ram_ptr += area.size;
				}
			}
		}

		for (auto &mp : modules) {
			for (auto &module : mp.second) {
				for (auto &area:  module.areas) {
					if (area.name!="_DATA") continue;
					if (area.type != Module::Area::RELATIVE) throw std::runtime_error(area.name + " not relative: " + module.filename);

					area.addr = ram_ptr;
					area.rom_addr = uint32_t(-1);
					ram_ptr += area.size;
				}
			}
		}

		for (auto &mp : modules) {
			for (auto &module : mp.second) {
				for (auto &area:  module.areas) {
					if (area.name!="_XDATA") continue;
					if (area.type != Module::Area::RELATIVE) throw std::runtime_error(area.name + " not relative: " + module.filename);

					area.addr = ram_ptr;
					area.rom_addr = uint32_t(-1);
					ram_ptr += area.size;
				}
			}
		}
	}

	// ALLOCATE BANKABLE CODE AREAS
	{	
		std::vector<std::pair<uint32_t,std::string>> bankableModules;
		
		for (auto &mp : modules) {
			bankableModules.emplace_back(0,mp.first);
			for (auto &module : mp.second) {
				for (auto &area:  module.areas) {
					if (area.name!="_CODE") continue;
					if (area.size==0) continue;
					if (module.page<0) throw std::runtime_error(module.name + " used but not allocated a page");
					if (area.type != Module::Area::RELATIVE) throw std::runtime_error(area.name + " not relative: " + module.filename);
					
					bankableModules.back().first += area.size;
				}
			}		
			if (bankableModules.back().first>0x2000) throw std::runtime_error("Module " + mp.first + " too large to fit a segment");
		}
		
		std::sort(bankableModules.begin(), bankableModules.end());
		std::reverse(bankableModules.begin(), bankableModules.end());

		std::vector<uint32_t> segments;
		segments.push_back(std::max(0U, 0x6000-rom_ptr));
		segments.push_back(std::max(0U, 0x8000-rom_ptr));
		segments.push_back(std::max(0U, 0xA000-rom_ptr));
		segments.push_back(std::max(0U, 0xC000-rom_ptr));
				
		for (auto& [size, name]: bankableModules) {
			
			uint32_t i;
			for (i=0; i<segments.size() and segments[i]<size; i++);
			if (i==segments.size()) 
				segments.push_back(0x2000);

			for (auto &module : modules[name]) {
				module.segment = i;
				for (auto &area:  module.areas) {
					if (area.name != "_CODE") continue;

					area.addr = 0x2000*(2+module.page) + 0x2000 - segments[i]; 
					area.rom_addr = 0x2000*(2+i) + 0x2000 - segments[i];

					segments[i] -= area.size;

					Log(2) << "Module: " << module.name << " addressed at: 0x" << std::hex << area.addr << std::dec << " (" << area.size << " bytes) in page: " << module.page << " and segment " << module.segment;
				}
			}
		}
	}
	
	
	// Generate area map
	{
		std::ofstream off(romName + ".areas.map");
		off << "AREA MAP: " << std::endl;
		off << "# SG #  MAP #  ROM  # SIZE #   NAME   #        HEADER        #        PAGE A        #        PAGE B        #        PAGE C        #        PAGE D        #" << std::endl;
		off << "##########################################################################################################################################################" << std::endl;
		for (uint32_t i=0; i<256; i++) {
			
			std::multimap<uint32_t, std::string> lines;
			
			for (auto &mp : modules) {
				for (auto &module : mp.second) {
					if (module.segment != (int)i) continue;
					for (auto &area:  module.areas) {
						if (area.size==0) continue;
							
						std::ostringstream oss;
						
						char s[200];
						if (area.rom_addr==uint32_t(-1)) {
							snprintf(s,199,"#%3X # %04X # ----- # %04X # %8.8s #",module.segment, area.addr, area.size, area.name.substr(1).c_str());
						} else {
							snprintf(s,199,"#%3X # %04X # %05X # %04X # %8.8s #",module.segment, area.addr, area.rom_addr, area.size, area.name.substr(1).c_str());
						}
						oss << s;
						for (int j=-1; j<module.page; j++) oss << "                      #";
						snprintf(s,199," %20.20s #",module.name.c_str());
						oss << s;	
						for (int j=module.page+1; j<4; j++) oss << "                      #";
						lines.emplace(area.addr,oss.str());
						
					}	
				}
			}
			for (auto &&s : lines)
				off << s.second << std::endl;
			if (not lines.empty()) 
				off << "##########################################################################################################################################################" << std::endl;

		}
	}

	// Generate symbols map
	{
		std::ofstream off(romName + ".symbols.map");
		off << "Symbols MAP: " << std::endl;
		off << "# SG #  MAP #  ROM  #  MODULE  #        HEADER        #        PAGE A        #        PAGE B        #        PAGE C        #        PAGE D        #" << std::endl;
		off << "###################################################################################################################################################" << std::endl;
		for (uint32_t i=0; i<256; i++) {
			
			std::multimap<uint32_t, std::string> lines;
			
			for (auto &mp : modules) {
				for (auto &module : mp.second) {
					if (module.segment != (int)i) continue;
					for (auto &area:  module.areas) {
						if (area.size==0) continue;

						for (auto &symbol : module.symbols) {
							if (symbol.type != Module::Symbol::DEF) continue;
							if (symbol.areaName != area.name) continue;
							
							std::ostringstream oss;
							
							char s[200];
							if (area.rom_addr==uint32_t(-1)) {
								snprintf(s,199,"#%3X # %04X # ----- # %-8.8s #",module.segment, area.addr + symbol.addr, module.name.c_str());
							} else {
								snprintf(s,199,"#%3X # %04X # %05X # %-8.8s #",module.segment, area.addr + symbol.addr, area.rom_addr + symbol.addr, module.name.c_str());
							}
							oss << s;
							for (int j=-1; j<module.page; j++) oss << "                      #";
							snprintf(s,199," %-20.20s #",symbol.name.c_str());
							oss << s;	
							for (int j=module.page+1; j<4; j++) oss << "                      #";
							lines.emplace(area.addr,oss.str());
						
						}
					}	
				}
			}
			for (auto &&s : lines)
				off << s.second << std::endl;
			if (not lines.empty()) 
				off << "###################################################################################################################################################" << std::endl;

		}
	}

	Log(2) << "Allocated: " << (rom_ptr-0x4000) << " bytes of ROM";
	if (rom_ptr>0xC000) throw std::runtime_error("Main segment ROM doesn't fit 32KB");

	Log(2) << "Allocated: " << (ram_ptr-megalinkerSymbols["___ML_CONFIG_RAM_START"]) << " bytes of RAM";		
	if (ram_ptr>0xF000) throw std::runtime_error("Ram area dangerously close to stack.");
	
	// DO LABEL SYMBOL ADDRESSES
	std::map<std::string,uint32_t> symbolsAddress;
	for (auto &mp : modules) {
		for (auto &module : mp.second) {
			std::map<std::string, uint32_t> areaAddress;
			for (auto &area:  module.areas) 
				areaAddress[area.name] = area.addr;
				
			for (auto &symbol : module.symbols) {
				if (symbol.type == Module::Symbol::DEF) {
					symbolsAddress[symbol.name] = areaAddress[symbol.areaName] + symbol.addr;
					symbol.absoluteAddress = symbolsAddress[symbol.name];
					if (symbol.name[0]!='.') 
						Log(2) << "Symbol: " << symbol.name << " defined at: 0x" << std::hex << symbol.absoluteAddress << std::dec << " at page: " << module.page;
				}
			}
		}
	}
	
	// DO EXTRACT THE CODE
	std::vector<uint8_t> rom(0x20000,0xff);
	for (auto &mp : modules) {
		for (auto &module : mp.second) {
		
			std::istringstream isf(module.content);
			std::string line;
			
			uint32_t current_area=0;
			std::vector<int> area_addr;
			std::vector<int> area_rom_addr;
			for (auto &area : module.areas) {
				if (area.type == Module::Area::RELATIVE) {
					area_addr.push_back(area.addr); 
					area_rom_addr.push_back(area.rom_addr); 
				} else {
					if (area.size)
						Log(3) << "Module: " << module.name << " Area: " << area.name << " " << area.addr << " " << area.rom_addr;
					area_addr.push_back(0);
					area_rom_addr.push_back(0);
				}
			}
				
			uint32_t last_t_pos=0;
			std::vector<uint8_t> T;
			
			while (std::getline(isf, line)) {
				
				std::istringstream isl(line);
				std::string type;
				isl >> type;

				if (type=="XL2") { // HEADER
				} else if (type=="M") { // NOT HERE
				} else if (type=="O") { // NOT NEEDED
				} else if (type=="H") { // NOT NEEDED
				} else if (type=="S") { // NOT HERE
				} else if (type=="A") { // NOT HERE
				} else if (type=="T") { // HERE
					
					uint32_t xx0, xx1;
					isl >> HEX(xx0,HEX::TWO_NIBBLES) >> HEX(xx1,HEX::TWO_NIBBLES);
					last_t_pos = xx1*0x100 + xx0;
					
					T.clear();
					uint32_t nn;
					while (isl >> HEX(nn,HEX::TWO_NIBBLES))
						T.push_back(nn);
					
				} else if (type=="R") { // HERE

					uint32_t aa0, aa1;
					isl >> AR("00") >> AR("00") >> HEX2(aa0) >> HEX2(aa1);
					current_area = aa1*0x100 + aa0;

					uint32_t n1, n2, xx0, xx1;
					uint32_t n2Adjust = 2;
					while (isl >> HEX2(n1) >> HEX2(n2) >> HEX2(xx0) >> HEX2(xx1)) {

						enum { 
							R3_WORD=0x00, R3_BYTE=0x01, 
							R3_AREA=0x00, R3_SYM =0x02, 
							R3_NORM=0x00, R3_PCR =0x04, 
							R3_BYT1=0x00, R3_BYTX=0x08, 
							R3_SGND=0x00, R3_USGN=0x10,
							R3_LSB =0x00, R3_MSB =0x80
						};
						
						uint32_t idx = xx1*0x100 + xx0;
						uint32_t address = 0;
						
						if (n2 <n2Adjust) 
							throw std::runtime_error("n2 < n2Adjust??");
						n2-=n2Adjust;

						
						if ( n1 & R3_SYM ) {
							
							if (symbolsAddress.count(module.symbols[idx].name)!=0)  {

								address = symbolsAddress[module.symbols[idx].name];
							} else if (module.symbols[idx].isSegmentSymbol()) {
								
								std::string requestedModule = module.symbols[idx].getSegmentName();
								address = modules[requestedModule].front().segment;
								
								Log(2) << "Current area: " << module.areas[current_area].name << " (" << module.page << ") is loading " << module.symbols[idx].name << " (" << modules[requestedModule].front().page << ")" ;
								if (module.areas[current_area].name == "_CODE" and module.page == modules[requestedModule].front().page) 
									Log(3) << "Warning: In module " << module.name << " and area: " << module.areas[current_area].name << " (" << module.page << ") is loading " << module.symbols[idx].name << " (" << modules[requestedModule].front().page << ")" ;
								
							
							} else if (module.symbols[idx].isConfigurationSymbol()) {
							
								address = megalinkerSymbols[module.symbols[idx].name];
							} else {
							
								throw std::runtime_error("Undefined symbol: " + module.symbols[idx].name); 
							}
							
							Log(3) << module.symbols[idx].name << " " << std::hex << address;
							
							n1 -= R3_SYM;
						} else  {
						
							address = area_addr[idx];
						}
						
						
						if        (n1 == R3_WORD ) {

							address += T[n2+0] + T[n2+1]*0x100;
							
							T[n2+0] = address & 0xFF;
							T[n2+1] = address >> 8;
						
						} else if (n1 == R3_BYTE + R3_BYTX + R3_LSB) {
							
							address += T[n2+0] + T[n2+1]*0x100;

							for (uint32_t i=n2+1; i<T.size(); i++) 
								T[i-1] = T[i];
							T.pop_back();

							T[n2+0] = address & 0xFF;
							
							n2Adjust++;

						} else if (n1 == R3_BYTE + R3_BYTX + R3_MSB) {
							
							address += T[n2+0] + T[n2+1]*0x100;

							for (uint32_t i=n2+1; i<T.size(); i++) 
								T[i-1] = T[i];
							T.pop_back();

							T[n2+0] = (address>>8) & 0xFF;
							
							n2Adjust++;


						} else {
							Log(3) << "N1: 0x"<< std::hex << n1 << std::dec;
							throw std::runtime_error("Unsupported relocation flag combination");
						}
					}	


					if (T.size())
						while (rom.size() < last_t_pos + area_rom_addr[current_area] - 0x4000 + T.size()) 
							rom.resize(rom.size()+0x2000,0xff);

					for (auto &t : T) rom[area_rom_addr[current_area] - 0x4000 + last_t_pos++] = t;

				} else if (not type.empty()) {
					
					std::runtime_error("Unrecognized type: " + type);
				}
			}
		}
	}


	// DO WRITE THE ROM
	{
		std::ofstream off(romName);
		off.write((const char *)&rom[0x0000],rom.size()-0x0000);
	}
		
    {
        printf("Using %u bytes of ram, from 0x%04X to 0x%04X.\n", 
			uint32_t(megalinkerSymbols["___ML_CONFIG_INIT_RAM_SIZE"]),
			uint32_t(megalinkerSymbols["___ML_CONFIG_INIT_RAM_START"]), 
			uint32_t(megalinkerSymbols["___ML_CONFIG_INIT_RAM_END"]));
    }

	return 0;
}
