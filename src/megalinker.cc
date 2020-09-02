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

struct REL {
	
	
	struct AREA {
		
		std::string name;
		uint32_t size;
		uint32_t addr;
		uint32_t rom_addr;
		enum { ABSOLUTE, RELATIVE} type;
		
	};

	struct SYMBOL {

        const std::string prefix = "___ML_";
        
        bool isMegalinkerSymbol() const { 
            
            if (name.size() < prefix.size()) return false;
            if (name.substr(0,prefix.size()) != prefix) return false;
            return true;
        }

        bool isModuleAddressSymbol() const { 
            
            if (name.size() < prefix.size()+3) return false;
            if (name.substr(0,prefix.size()) != prefix) return false;
            if (name[prefix.size()+1]!='_') return false;
            if (name[prefix.size()] < 'A' or name[prefix.size()] > 'D') throw std::runtime_error("Module Symbol: " + name + " requires a wrong page");
            return true;
        }
        
        std::string moduleName() const { 
			if (not isModuleAddressSymbol()) throw std::runtime_error("Module Symbol: " + name + " is not a module address symbol");
			return name.substr(prefix.size()+2); 
		}
        int page() const { 
			if (not isModuleAddressSymbol()) throw std::runtime_error("Module Symbol: " + name + " is not a module address symbol");
			return name[prefix.size()]-'A'; 
		}
		
		std::string name;
		uint32_t addr;
		enum { DEF, REF} type;
		std::string areaName;
		
		uint32_t absoluteAddress;
	};
	
	std::string filename, name, content;
	std::vector<AREA> areas;
	std::vector<SYMBOL> symbols;
	
	bool enabled = false;
	int page = -1;
	int segment = 0;
};

////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
	
	Log::reportLevel(0);
	
	std::set<std::string> known_areas = { 
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

	std::string romName = "out.rom";
	std::vector<REL> rels;
	
	// PREPROCESS ALL INPUT FILES
	for (int i=1; i<argc; i++) {
		
		std::string arg = argv[i];
		
		if (arg.substr(arg.find_last_of(".")) == ".rom") {

			Log(1) << "Rom name: " << arg;
			romName = arg;
			
		} else if (arg.substr(arg.find_last_of(".")) == ".rel") {	
			
			Log(1) << "Processing: " << arg;
			REL rel;
			rel.filename = arg;
			
			std::ifstream isf(arg);
			std::stringstream buffer;
			buffer << isf.rdbuf();
			rel.content = buffer.str();
			
			rels.push_back(rel);

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
				
				REL rel;
				rel.filename = ar_file_name;
				rel.content.resize(ar_file_size);
				isf.read(&rel.content[0],ar_file_size); 

				if (!isf) break;
				if (!isf) throw std::runtime_error("library terminates before reading entire file");
				
				
				if (rel.content.size()>10 and rel.content.substr(0,3)=="XL2") {
					rels.push_back(rel);
				} else {
					Log(2) << "File " << ar_file_name << " not a relocatable object file";
				} 
				
				if (ar_file_size % 2 == 1) isf.get(); // Align to 2
			}
		}
	}
	
	for (auto &&rel : rels) {
		
		std::istringstream isf(rel.content);
		std::string line;

		Log(2) << "File name: " << rel.filename; 
		
		while (std::getline(isf, line)) {
			
			std::istringstream isl(line);
			std::string type;
			isl >> type;


			if (type=="XL2") { // DEFAULT
			} else if (type=="M") { // NOT REQUIRED
				
				isl >> rel.name;
				Log(1) << "Module name: " << rel.name << " (" << rel.filename << ")"; 
				
			} else if (type=="O") { // NOT NEEDED
			} else if (type=="H") { // NOT NEEDED
			} else if (type=="S") {
				
				REL::SYMBOL symbol;
				std::string st = "   ";
				
				isl >> symbol.name >> st[0] >> st[1] >> st[2] >> HEX(symbol.addr, HEX::PLAIN);
				
					
				if (st=="Def") {
					symbol.type = REL::SYMBOL::DEF;
				} else if (st=="Ref") {
					symbol.type = REL::SYMBOL::REF;
				} else throw std::runtime_error("Symbol type unexpected");
				
				if (not rel.areas.empty())
					symbol.areaName = rel.areas.back().name;
				
				rel.symbols.push_back(symbol);
				
			} else if (type=="A") {
				
				REL::AREA area;
				
				uint32_t flags;
				isl >> area.name >> AR("size") >> HEX(area.size,HEX::PLAIN) 
					>> AR("flags") >> flags 
					>> AR("addr") >> HEX(area.addr,HEX::PLAIN);
					
				if (area.name.size()>0 and area.name[0]!='_')
					area.name = '_' + area.name;
				                
				if (flags==0) {
					area.type = REL::AREA::RELATIVE;
				} else if (flags==8) {
					area.type = REL::AREA::ABSOLUTE;
				} else throw std::runtime_error("Unexpected flag");
				
				
				if (area.size>0) Log(1) << "Found area: " << area.name << " of size: " << area.size;
				if (area.size>0 and known_areas.count(area.name) == 0) throw std::runtime_error("Area " + area.name +" unknown");
				
				if (area.name=="_HEADER0") {
					rel.enabled=true;
				}

				rel.areas.push_back(area);
				
			} else if (type=="T") { // NOT NOW
			} else if (type=="R") { // NOT NOW
			} else if (not type.empty()) {
				
				throw std::runtime_error("Unrecognized type: " + type);
			}
		}
	}

	// FAST ERROR CHECKING
	if (rels.empty()) throw std::runtime_error("No files to parse");
	
	
	// ENABLE ALL REQUIRED FILES / MODULES
	for (;;) {
		
		bool updated = false;

		std::map<std::string,int> referencedSymbols;
		std::set<std::string> definedSymbols;

		for (auto &rel : rels) {
			if (not rel.enabled) continue;
			for (auto &sym : rel.symbols) {
				if (sym.type!=REL::SYMBOL::REF) continue;
				if (sym.isModuleAddressSymbol()) {
					for (auto &rel2 : rels) {
						if (rel2.enabled) continue;
						if (rel2.name != sym.moduleName()) continue;
						rel2.enabled = true;
						updated = true;
					}
					continue;
				}
				if (sym.isMegalinkerSymbol()) continue;
				referencedSymbols[sym.name] = 0;
			}
		}

		for (auto &rel : rels) {
			for (auto &sym : rel.symbols) {
				if (sym.type!=REL::SYMBOL::DEF) continue;
					
				if (not rel.enabled and referencedSymbols.count(sym.name)) {
					rel.enabled = true;
					updated = true;
				}
				
				if (rel.enabled and referencedSymbols.count(sym.name)) {
					if (definedSymbols.count(sym.name)) throw std::runtime_error("Symbol: " + sym.name + "defined multiple times");
					definedSymbols.insert(sym.name);
					referencedSymbols[sym.name]++;
				}
			}
		}
		
		for (auto &ref : referencedSymbols)
			if (ref.second==0)
				throw std::runtime_error("Referenced Symbol: " + ref.first + " not defined");
				
		if (not updated) break;
	}

	
	// PAGE ALLOCATION AND ERROR CHECKING
	{
		std::map<std::string,REL *> modulesByName;

		for (auto &&rel : rels)
			if (rel.enabled)
				modulesByName.emplace(rel.name,&rel);
				
		for (auto &&mod : modulesByName)
			Log(3) << mod.first;

		for (auto &&rel : rels) {
			if (not rel.enabled) continue;
			for (auto &sym : rel.symbols) {
				if (sym.type!=REL::SYMBOL::REF) continue; 
				if (not sym.isModuleAddressSymbol()) continue;
								
				if (modulesByName.count(sym.moduleName())==0)
					throw std::runtime_error("Module " + sym.moduleName() + " unknown");
				
				if (modulesByName[sym.moduleName()]->page==-1)
					modulesByName[sym.moduleName()]->page = sym.page();
					
				if (modulesByName[sym.moduleName()]->page != sym.page())
					throw std::runtime_error("Module " + sym.moduleName() + " required at different pages");
			}
		}

		// TODO: Move this warning once we know that the symbol is required at not HOME section
		for (auto &&rel : rels) {
			if (not rel.enabled) continue;
			for (auto &sym : rel.symbols) {
				if (sym.type!=REL::SYMBOL::REF) continue; 
				if (not sym.isModuleAddressSymbol()) continue;
				
				if (modulesByName[sym.moduleName()]->page == rel.page)
					std::cerr << "Warning: Module " << rel.name << " is loading " << sym.moduleName() << " in its own page" << std::endl;
			}
		}
	}
	
	std::map<std::string, uint32_t> megalinkerSymbols;
	// FINDING ALL MEGALINKER DEFINED SYMBOLS
	{
		for (auto &&rel : rels) {
			if (not rel.enabled) continue;
			for (auto &sym : rel.symbols) {
				if (sym.type!=REL::SYMBOL::DEF) continue;
				if (not sym.isMegalinkerSymbol()) continue;
				megalinkerSymbols[sym.name] = sym.addr;
			}
		}
	}
	uint32_t rom_ptr = -1;
	uint32_t ram_ptr = -1;
	if (megalinkerSymbols.count("___ML_RAM_START")==0) throw std::runtime_error("___ML_RAM_START not defined");
	ram_ptr = megalinkerSymbols["___ML_RAM_START"];
	
	// ALLOCATE ALL NON BANKABLE AREAS
	{
		for (auto &rel : rels) {
			if (not rel.enabled) continue;
			for (auto &area:  rel.areas) {
				if (area.name!="_HEADER0") continue;
				if (rom_ptr!=uint32_t(-1)) throw std::runtime_error(area.name + " defined more than once: " + rel.filename);
				if (area.type != REL::AREA::ABSOLUTE) throw std::runtime_error(area.name + " not absolute: " + rel.filename);
				if (area.addr != 0x4000) throw std::runtime_error("HEADER not at 0x4000: " + rel.filename);
	
				rom_ptr = area.addr;
				area.rom_addr = area.addr;
				rom_ptr += area.size;
			}
		}
		
		for (auto &rel : rels) {
			if (not rel.enabled) continue;
			for (auto &area:  rel.areas) {
				if (area.name!="_GSINIT") continue;
				if (area.type != REL::AREA::RELATIVE) throw std::runtime_error(area.name + " not relative: " + rel.filename);

				area.addr = rom_ptr;
				area.rom_addr = area.addr;
				rom_ptr += area.size;
			}
		}

		for (auto &rel : rels) {
			if (not rel.enabled) continue;
			for (auto &area:  rel.areas) {
				if (area.name!="_GSFINAL") continue;
				if (area.type != REL::AREA::RELATIVE) throw std::runtime_error(area.name + " not relative: " + rel.filename);

				area.addr = rom_ptr;
				area.rom_addr = area.addr;
				rom_ptr += area.size;
			}
		}

		megalinkerSymbols["___ML_INIT_ROM_START"] = rom_ptr;
		megalinkerSymbols["___ML_INIT_RAM_START"] = ram_ptr;

		for (auto &rel : rels) {
			if (not rel.enabled) continue;
			for (auto &area:  rel.areas) {
				if (area.name!="_HOME") continue;
				if (area.type != REL::AREA::RELATIVE) throw std::runtime_error(area.name + " not relative: " + rel.filename);

				area.addr = ram_ptr;
				area.rom_addr = rom_ptr;
				rom_ptr += area.size;
				ram_ptr += area.size;
			}
		}


		for (auto &rel : rels) {
			if (not rel.enabled) continue;
			for (auto &area:  rel.areas) {
				if (area.name!="_INITIALIZER") continue;
				if (area.type != REL::AREA::RELATIVE) throw std::runtime_error(area.name + " not relative: " + rel.filename);

				area.addr = rom_ptr;
				area.rom_addr = area.addr;
				rom_ptr += area.size;
			}
		}
		megalinkerSymbols["___ML_INIT_SIZE"] = rom_ptr - megalinkerSymbols["___ML_INIT_ROM_START"];

		for (auto &rel : rels) {
			if (not rel.enabled) continue;
			for (auto &area:  rel.areas) {
				if (area.name!="_INITIALIZED") continue;
				if (area.type != REL::AREA::RELATIVE) throw std::runtime_error(area.name + " not relative: " + rel.filename);

				area.addr = ram_ptr;
				area.rom_addr = uint32_t(-1);
				ram_ptr += area.size;
			}
		}

		for (auto &rel : rels) {
			if (not rel.enabled) continue;
			for (auto &area:  rel.areas) {
				if (area.name!="_DATA") continue;
				if (area.type != REL::AREA::RELATIVE) throw std::runtime_error(area.name + " not relative: " + rel.filename);

				area.addr = ram_ptr;
				area.rom_addr = uint32_t(-1);
				ram_ptr += area.size;
			}
		}		

		for (auto &rel : rels) {
			if (not rel.enabled) continue;
			for (auto &area:  rel.areas) {
				if (area.name!="_XDATA") continue;
				if (area.type != REL::AREA::RELATIVE) throw std::runtime_error(area.name + " not relative: " + rel.filename);

				area.addr = ram_ptr;
				area.rom_addr = uint32_t(-1);
				ram_ptr += area.size;
			}
		}		
	}

	// ALLOCATE BANKABLE CODE AREAS
	{	
		std::vector<std::pair<uint32_t,std::reference_wrapper<REL>>> bankableRels;
		
		for (auto &rel : rels) {
			if (not rel.enabled) continue;
//			if (not rel.page!=0) continue;
			for (auto &area:  rel.areas) {
				if (area.name!="_CODE") continue;
				if (area.size==0) continue;
				if (rel.page<0) throw std::runtime_error(rel.name + " used but not allocated a page");
				if (area.type != REL::AREA::RELATIVE) throw std::runtime_error(area.name + " not relative: " + rel.filename);
				
				bankableRels.emplace_back(area.size,std::ref(rel));
				
				if (area.size>0x2000) throw std::runtime_error("File: " + rel.name + " too large to fit a segment");
			}
		}
		
		std::sort(bankableRels.begin(), bankableRels.end(), 
		[](const std::pair<uint32_t,std::reference_wrapper<REL>> &rhs,const std::pair<uint32_t,std::reference_wrapper<REL>> &lhs){
			return rhs.first < lhs.first;
		});
		std::reverse(bankableRels.begin(), bankableRels.end());

		std::vector<uint32_t> segments;
		segments.push_back(std::max(0U, 0x6000-rom_ptr));
		segments.push_back(std::max(0U, 0x8000-rom_ptr));
		segments.push_back(std::max(0U, 0xA000-rom_ptr));
		segments.push_back(std::max(0U, 0xC000-rom_ptr));
				
		for (auto &prel: bankableRels) {
			uint32_t i;
			for (i=0; i<segments.size() and segments[i]<prel.first; i++);
			if (i==segments.size()) 
				segments.push_back(0x2000);

			for (auto &a : prel.second.get().areas) {
				if (a.name=="_CODE") {
					a.addr = 0x2000*(2+prel.second.get().page) + 0x2000 - segments[i]; 
					a.rom_addr = 0x2000*(2+i) + 0x2000 - segments[i];
				}
			}
			
			segments[i] -= prel.first;
			
			prel.second.get().segment = i;
			
			for (auto &a : prel.second.get().areas)
				if (a.name=="_CODE")
					Log(2) << "Module: " << prel.second.get().name << " addressed at: 0x" << std::hex << a.addr << std::dec << " (" << prel.first << " bytes) in page: " << prel.second.get().page << " and segment " << prel.second.get().segment;
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
			
			for (auto &rel : rels) {
				if (rel.segment != (int)i) continue;
				if (not rel.enabled) continue;
				for (auto &area:  rel.areas) {
					if (area.size==0) continue;
						
					std::ostringstream oss;
					
					char s[200];
					if (area.rom_addr==uint32_t(-1)) {
						snprintf(s,199,"#%3X # %04X # ----- # %04X # %8.8s #",rel.segment, area.addr, area.size, area.name.substr(1).c_str());
					} else {
						snprintf(s,199,"#%3X # %04X # %05X # %04X # %8.8s #",rel.segment, area.addr, area.rom_addr, area.size, area.name.substr(1).c_str());
					}
					oss << s;
					for (int j=-1; j<rel.page; j++) oss << "                      #";
					snprintf(s,199," %20.20s #",rel.name.c_str());
					oss << s;	
					for (int j=rel.page+1; j<4; j++) oss << "                      #";
					lines.emplace(area.addr,oss.str());
					
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
			
			for (auto &rel : rels) {
				if (rel.segment != (int)i) continue;
				if (not rel.enabled) continue;
				for (auto &area:  rel.areas) {
					if (area.size==0) continue;

					for (auto &symbol : rel.symbols) {
						if (symbol.type != REL::SYMBOL::DEF) continue;
						if (symbol.areaName != area.name) continue;
						
						std::ostringstream oss;
						
						char s[200];
						if (area.rom_addr==uint32_t(-1)) {
							snprintf(s,199,"#%3X # %04X # ----- # %-8.8s #",rel.segment, area.addr + symbol.addr, rel.name.c_str());
						} else {
							snprintf(s,199,"#%3X # %04X # %05X # %-8.8s #",rel.segment, area.addr + symbol.addr, area.rom_addr + symbol.addr, rel.name.c_str());
						}
						oss << s;
						for (int j=-1; j<rel.page; j++) oss << "                      #";
						snprintf(s,199," %-20.20s #",symbol.name.c_str());
						oss << s;	
						for (int j=rel.page+1; j<4; j++) oss << "                      #";
						lines.emplace(area.addr,oss.str());
					
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

	Log(2) << "Allocated: " << (ram_ptr-megalinkerSymbols["___ML_RAM_START"]) << " bytes of RAM";		
	if (ram_ptr>0xF000) throw std::runtime_error("Ram area dangerously close to stack.");
	
	// DO LABEL SYMBOL ADDRESSES
	std::map<std::string,uint32_t> symbolsAddress;
	for (auto &rel : rels) {
		if (not rel.enabled) continue;
		std::map<std::string,uint32_t> areaAddress;
		for (auto &area:  rel.areas) 
			areaAddress[area.name] = area.addr;
			
		for (auto &symbol : rel.symbols) {
			if (symbol.type == REL::SYMBOL::DEF) {
				symbolsAddress[symbol.name] = areaAddress[symbol.areaName] + symbol.addr;
				symbol.absoluteAddress = symbolsAddress[symbol.name];
				if (symbol.name[0]!='.') 
					Log(2) << "Symbol: " << symbol.name << " defined at: 0x" << std::hex << symbol.absoluteAddress << std::dec << " at page: " << rel.page;
			}
		}
	}
	
	// DO EXTRACT THE CODE
	std::vector<uint8_t> rom(0x20000,0xff);
	for (auto &rel : rels) {		
		if (not rel.enabled) continue;
		
		std::ifstream isf(rel.filename);
		std::string line;
		
		uint32_t current_area=0;
		std::vector<int> area_addr;
		std::vector<int> area_rom_addr;
		for (auto &area : rel.areas) {
		//		std::cerr << rel.name << " " << area.name << " " << area.rom_addr << " " << area.size << std::endl;
			if (area.type == REL::AREA::RELATIVE) {
				area_addr.push_back(area.addr); 
				area_rom_addr.push_back(area.rom_addr); 
			} else {
				if (area.size)
					Log(3) << "Module: " << rel.name << " Area: " << area.name << " " << area.addr << " " << area.rom_addr;
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

			if (type=="XL2") { // DEFAULT
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
						
						if (symbolsAddress.count(rel.symbols[idx].name)!=0)  {

							address = symbolsAddress[rel.symbols[idx].name];
						} else if (rel.symbols[idx].isModuleAddressSymbol()) {
							
							std::string requested_module = rel.symbols[idx].moduleName();
							//Log(10) << "Looking for module: " << requested_module;
							for (auto &rel2 : rels) {
								if (rel2.name == requested_module) {
									address = rel2.segment;
									Log(0) << "FOUND! for module: " << requested_module << " " << rel2.name << " " << rel2.segment;
								}
							}
						
						} else if (rel.symbols[idx].isMegalinkerSymbol()) {
						
							address = megalinkerSymbols[rel.symbols[idx].name];
						} else {
						
							throw std::runtime_error("Undefined symbol: " + rel.symbols[idx].name); 
						}
						
						Log(3) << rel.symbols[idx].name << " " << std::hex << address;
						
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


//				std::cerr << rel.name << " " << (last_t_pos +area_rom_addr[current_area]) << " " << T.size() << std::endl;

				if (T.size())
					while (rom.size() < last_t_pos + area_rom_addr[current_area] - 0x4000 + T.size()) 
						rom.resize(rom.size()+0x2000,0xff);
				
				//if (T.size()) Log(0) << rel.name << " " << std::hex << last_t_pos;

//				if (T.size()) Log(4) << std::hex << (last_t_pos - 0x2000*(rel.segment - rel.page)) << " " << rel.segment << " " << rel.page;
					
				
//				for (auto &t : T) rom[last_t_pos++] = t;
				for (auto &t : T) rom[area_rom_addr[current_area] - 0x4000 + last_t_pos++] = t;

			} else if (not type.empty()) {
				
				std::runtime_error("Unrecognized type: " + type);
			}
		}
	}


	// DO WRITE THE ROM
	{
		std::ofstream off(romName);
		off.write((const char *)&rom[0x0000],rom.size()-0x0000);
	}
		
    {
        printf("Using %d bytes of ram, from 0xC000 to 0x%04X.\n", int(ram_ptr-0xC000), int(ram_ptr));
    }

	return 0;
}
