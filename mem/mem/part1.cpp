#include <fstream>
#include <vector>
#include <iostream>
#include <string>
#include <bitset>

const long PAGE_SIZE = 256;

using namespace std;

struct mem_add {
	unsigned long virt;
	unsigned long page;
	unsigned long offset;
};

struct page {
	int pagenum;
	char data[256];
	bool modified = false;
};

string get_bitset(bitset<32> bit, int start, int length) {
	string bitstr = bit.to_string();
	reverse(bitstr.begin(), bitstr.end());
	string ret = bitstr.substr(start, length);
	reverse(ret.begin(), ret.end());
	return ret;
}

int page_hit(page page[], unsigned long pagen) {
	for (unsigned int i = 0; i < PAGE_SIZE; i++) {
		if (page[i].pagenum == pagen) return i;
	}
	return -1;
}

int main(int argc, char **argv) {
	if (argc < 3) {
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

	page pages[256];
	for (unsigned int i = 0; i < addresses.size(); i++) {
		int physadd = 0, val = 0;

		int hit = page_hit(pages, addresses.at(i).page);
		if (hit > 0) {
			physadd = (hit * PAGE_SIZE) + addresses.at(i).offset;
			val = pages[hit].data[addresses.at(i).offset];
		}

		else {
			struct page newpage;

			memory.seekg(addresses.at(i).page * PAGE_SIZE, ios::beg);
			memory.read(newpage.data, PAGE_SIZE);

			newpage.pagenum = addresses.at(i).page;
			for (unsigned int j = 0; j < 256; j++) {
				if (!pages[j].modified) {
					pages[j] = newpage;
					pages[j].modified = true;
					physadd = (j * PAGE_SIZE) + addresses.at(i).offset;
					val = newpage.data[addresses.at(i).offset];
					break;
				}
			}
		}

		correct << "Virtual address: " << addresses.at(i).virt << " Physical address: " << physadd << " Value: " << val << "\n";
	}
	memory.close();
	correct.close();
}