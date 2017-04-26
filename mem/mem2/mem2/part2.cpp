#include <fstream>
#include <vector>
#include <iostream>
#include <string>
#include <bitset>
#include <algorithm>

const long PAGE_SIZE = 256;
const long PAGE_COUNT = 128;
const long TLB_SIZE = 16;

using namespace std;

struct mem_add {
	unsigned long virt;
	unsigned long page;
	unsigned long offset;
};

struct page {
	int pagenum;
	char data[PAGE_SIZE];
	bool modified = false;
	int created;
	int used;
};

struct tlb_page {
	int pagenum;
	int index;
	bool modified = false;
	int used;
};

string get_bitset(bitset<32> bit, int start, int length) {
	string bitstr = bit.to_string();
	reverse(bitstr.begin(), bitstr.end());
	string ret = bitstr.substr(start, length);
	reverse(ret.begin(), ret.end());
	return ret;
}

int page_hit(page page[], unsigned long pagen) {
	for (unsigned int i = 0; i < PAGE_COUNT; i++) {
		if (long(page[i].pagenum) == pagen) return i;
	}
	return -1;
}

int tlb_hit(tlb_page tlb[], int pagen) {
	for (unsigned int i = 0; i < TLB_SIZE; i++) {
		if (tlb[i].pagenum == pagen) return i;
	}
	return -1;
}

int get_victim_tlb(tlb_page tlb[]) {
	int min = INT32_MAX;
	int index = -1;
	for (unsigned int i = 0; i < TLB_SIZE; i++) {
		if (tlb[i].used < min) {
			min = tlb[i].used;
			index = i;
		}
	}

	if (min == INT32_MAX) return -1;

	return index;
}

int get_victim_frame(page page[], string strategy)
{
	int index = -1;
	if(strategy.compare("fifo") == 0)
	{
		int min = INT32_MAX;
		for(unsigned int i = 0; i < PAGE_COUNT; i++)
		{
			if (page[i].created < min) {
				min = page[i].created;
				index = i;
			}
		}

		if (min == INT32_MAX) return -1;

		return index;
	}

	if (strategy.compare("lru") == 0)
	{
		int min = INT32_MAX;
		for(unsigned int i = 0; i < PAGE_COUNT; i++)
		{
			if (page[i].used < min) {
				min = page[i].used;
				index = i;
			}
		}

		if (min == INT32_MAX) return -1;

		return index;
	}
	
	cout << "Unknown strategy\n";
	return -1;
}

int main(int argc, char **argv) {
	if (argc < 4) {
		cout << "Not enough arguments\n";
		exit(1);
	}

	ifstream address;
	address.open(argv[2]);

	if (!address.is_open()) {
		cout << "Failed to read " << argv[2] << "\n";
		exit(1);
	}

	vector<mem_add> addresses;
	string line;
	while (getline(address, line)) {
		bitset<32> bit(stoi(line));
		mem_add mem;
		bitset<8> offset(get_bitset(bit, 0, 8));
		bitset<8> page(get_bitset(bit, 8, 8));
		mem.offset = offset.to_ulong();
		mem.page = page.to_ulong();
		mem.virt = stoul(line);

		addresses.push_back(mem);
	}
	address.close();

	ifstream memory;
	memory.open(argv[1], ios::binary | ios::in);
	if (!memory.is_open()) {
		cout << "Failed to read " << argv[1] << "\n";
		exit(1);
	}

	ofstream correct;
	correct.open("correct.txt");

	if (!correct.is_open()) {
		cout << "Failed to open correct.txt for writing\n";
		exit(1);
	}

	int faults = 0, tlbhits = 0;
	page pages[PAGE_COUNT];
	tlb_page tlb[TLB_SIZE];
	for (unsigned int i = 0; i < addresses.size(); i++) {
		int physadd = 0, val = 0, hit;

		if(i == 604)
		{
			int aaa = 0;
		}

		// TLB check
		int tlbhit = tlb_hit(tlb, addresses.at(i).page);
		if (tlbhit >= 0) {
			physadd = tlb[tlbhit].index * PAGE_SIZE + addresses.at(i).offset;
			val = pages[tlb[tlbhit].index].data[addresses.at(i).offset];
			pages[tlb[tlbhit].index].used = i;
			tlbhits++;
		}

		// TLB miss, Page table check
		else if ((hit = page_hit(pages, addresses.at(i).page)) >= 0) {
			physadd = hit * PAGE_SIZE + addresses.at(i).offset;
			val = pages[hit].data[addresses.at(i).offset];
			pages[hit].used = i;
		}

		// Page fault
		else {
			struct page newpage;
			memory.seekg(addresses.at(i).page * PAGE_SIZE, ios::beg);
			memory.read(newpage.data, PAGE_SIZE);

			newpage.pagenum = addresses.at(i).page;
			newpage.created = i;
			newpage.used = i;

			// Page table initial scheduling
			if (faults < PAGE_COUNT) {
				for (unsigned int j = 0; j < PAGE_COUNT; j++) {
					if (!pages[j].modified) {
						pages[j] = newpage;
						pages[j].modified = true;
						physadd = j * PAGE_SIZE + addresses.at(i).offset;
						val = newpage.data[addresses.at(i).offset];

						// TLB scheduling
						bool found = false;
						for (unsigned int k = 0; k < TLB_SIZE; k++) {
							if (!tlb[k].modified) {
								tlb[k].index = j;
								tlb[k].pagenum = newpage.pagenum;
								tlb[k].modified = true;
								tlb[k].used = i;

								found = true;
								break;
							}
						}
						
						// If the TLB is already full, replace existing
						if(!found) {
							int replace = get_victim_tlb(tlb);
							tlb[replace].index = j;
							tlb[replace].pagenum = newpage.pagenum;
							tlb[replace].modified = true;
							tlb[replace].used = i;
						}

						break;
					}
				}
				faults++;
			}

			// After page table is full, need replacement strategy
			else
			{
				int replace = get_victim_frame(pages, argv[3]);
				pages[replace] = newpage;
				pages[replace].modified = true;
				physadd = replace * PAGE_SIZE + addresses.at(i).offset;
				val = newpage.data[addresses.at(i).offset];
			}
		}
		correct << "Virtual address: " << addresses.at(i).virt << " Physical address: " << physadd << " Value: " << val << "\n";
	}

	memory.close();

	correct << "Number of Translated Addresses = " << addresses.size() << "\n";
	correct << "Page Faults = " << faults << "\n";
	correct << "Page Fault Rate = " << double(faults) / addresses.size() << "\n";
	correct << "TLB Hits = " << tlbhits << "\n";
	correct << "TLB Hit Rate = " << double(tlbhits) / addresses.size() << "\n";
	correct << "\n";

	correct.close();
}