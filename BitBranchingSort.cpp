#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>
#include <chrono>
#include <random>
using namespace std;

/* Test parameters (e.g., array sizes or value range) */
#define STARTING_ORDER_OF_MAGNITUDE 2 // The starting array size's order of magnitude (e.g., 2 will begin at size 100)
#define ENDING_ORDER_OF_MAGNITUDE 7 // The ending array size's order of magnitude (e.g., 7 will begin at size 10,000,000)
#define RETRY_COUNT_FOR_AVERAGE 10 // The number of retries per array size (e.g., 10). An average performance time will be calculated
#define MAX_VALUE 2147483646 // The max range of used numbers (e.g., 10, the lowst possible values is 0, as only positive integers are supported in this implementaion)
#define SORTED false // Whether or not the input array is sorted

#define KEY_SIZE 32 // The key size for integers, used in the below tests

/* Below definitions call comiler-specifc function for the purpose of counting leading/trailing zeroes in numbers */
int countLeadingZeros(uint32_t x) {
	if (x == 0) {
		return KEY_SIZE;
	}

#if defined(_MSC_VER)
	unsigned long index;
	_BitScanReverse(&index, x);
	return KEY_SIZE - 1 - index;
#else
	return __builtin_clz(x);
#endif
}

int countTrailingZeros(uint32_t x) {
	if (x == 0) {
		return KEY_SIZE;
	}

#if defined(_MSC_VER)
	unsigned long index;
	_BitScanForward(&index, x);
	return index;
#else
	return __builtin_ctz(x);
#endif
}

/* The bit branching tree node class */
class BitBranchingTreeNode
{
public:
	int branchIndices[KEY_SIZE];
	unsigned int reservedBranchesBitMask = 0;
	int count = 1;
	int value;
};

BitBranchingTreeNode* nodes;
int nodesSize = 0;

static void inOrderTraversal(vector<int>& sortedArray, int nodeIndex = 0)
{
	BitBranchingTreeNode* node = &nodes[nodeIndex];
	unsigned int branchesTo1sBitMask = node->reservedBranchesBitMask & ~(node->value);
	unsigned int branchesTo0sBitMask = node->reservedBranchesBitMask & node->value;

	while (branchesTo0sBitMask != 0)
	{
		unsigned int branchIndex = KEY_SIZE - 1 - countLeadingZeros(branchesTo0sBitMask);
		inOrderTraversal(sortedArray, node->branchIndices[branchIndex]);
		branchesTo0sBitMask ^= 1 << branchIndex;
	}

	for (int i = 0; i < node->count; i++)
	{
		sortedArray.push_back(node->value);
	}

	while (branchesTo1sBitMask != 0)
	{
		unsigned int branchIndex = countTrailingZeros(branchesTo1sBitMask);
		inOrderTraversal(sortedArray, node->branchIndices[branchIndex]);
		branchesTo1sBitMask ^= 1 << branchIndex;
	}
}

static void insertValue(int value)
{
	if (nodesSize == 0)
	{
		BitBranchingTreeNode* root = &nodes[nodesSize++];
		root->value = value;
		return;
	}

	BitBranchingTreeNode* current = &nodes[0];

	while (true)
	{
		unsigned int bitDifference = current->value ^ value;
		unsigned int matchingBitsCount = countLeadingZeros(bitDifference);

		if (matchingBitsCount == KEY_SIZE)
		{
			current->count++;
			return;
		}

		unsigned int branchingIndex = KEY_SIZE - 1 - matchingBitsCount;
		unsigned int branchingBit = 1 << branchingIndex;
		bool branchAlreadyExists = branchingBit & current->reservedBranchesBitMask;

		if (branchAlreadyExists)
		{ // Go there
			current = &nodes[current->branchIndices[branchingIndex]];
		}
		else
		{ // Make it
			current->reservedBranchesBitMask |= branchingBit; // Marks the branch as reserved
			current->branchIndices[branchingIndex] = nodesSize;
			BitBranchingTreeNode* branch = &nodes[nodesSize++];
			branch->value = value;
			return;
		}
	}
}

static vector<int> bitTreeSort(const vector<int>& array)
{
	nodes = new BitBranchingTreeNode[array.size()];
	nodesSize = 0;

	for (int value : array)
	{
		insertValue(value);
	}

	vector<int> sortedArray;
	sortedArray.reserve(array.size());
	inOrderTraversal(sortedArray);

	delete[] nodes;
	return sortedArray;
}

static bool isSorted(const vector<int>& array)
{
	for (size_t i = 1; i < array.size(); ++i)
	{
		if (array[i] < array[i - 1])
		{
			return false;
		}
	}
	return true;
}

void lsdRadixSort(vector<int>& array) {
	const int BITS_IN_BYTE = 8;
	const int NUM_BYTES = 4;
	const int RADIX = 1 << BITS_IN_BYTE;
	const int MASK = RADIX - 1;

	vector<int> buffer(array.size());

	for (int byteIndex = 0; byteIndex < NUM_BYTES; ++byteIndex) {
		vector<int> count(RADIX, 0);

		for (int value : array) {
			int currentByte = (value >> (byteIndex * BITS_IN_BYTE)) & MASK;
			count[currentByte]++;
		}

		for (int i = 1; i < RADIX; ++i) {
			count[i] += count[i - 1];
		}

		for (int i = array.size() - 1; i >= 0; --i) {
			int currentByte = (array[i] >> (byteIndex * BITS_IN_BYTE)) & MASK;
			buffer[--count[currentByte]] = array[i];
		}

		copy(buffer.begin(), buffer.end(), array.begin());
	}
}

int main()
{
	for (int i = STARTING_ORDER_OF_MAGNITUDE; i < ENDING_ORDER_OF_MAGNITUDE; ++i)
	{
		double bitBranchingSortTotalTime = 0.0;
		double lsdRadixSortTotalTime = 0.0;
		double heapSortTotalTime = 0.0;
		double quickSortTotalTime = 0.0;
		double stableSortTotalTime = 0.0;

		int size = 1;
		for (int j = 0; j < i; ++j)
		{
			size *= 10;
		}

		for (int k = 0; k < RETRY_COUNT_FOR_AVERAGE; ++k) {
			vector<int> array;
			random_device rd;
			mt19937 gen(rd());
			uniform_int_distribution<> dis(0, size / 100);
			for (int j = 0; j < size; ++j)
			{
				array.push_back(dis(gen));
			}

			if (SORTED)
			{
				sort(array.begin(), array.end());
			}

			// Measure bit branching sort execution time (average over 10 runs)
			auto start = chrono::high_resolution_clock::now();
			vector<int> sortedArray = bitTreeSort(array);
			auto end = chrono::high_resolution_clock::now();
			chrono::duration<double> elapsed = end - start;
			bitBranchingSortTotalTime += elapsed.count();

			assert(isSorted(sortedArray));
			assert(sortedArray.size() == array.size());

			// Measure radix sort execution time (average over 10 runs)
			vector<int> radixSortedArray = array;
			start = chrono::high_resolution_clock::now();
			lsdRadixSort(radixSortedArray);
			end = chrono::high_resolution_clock::now();
			elapsed = end - start;
			lsdRadixSortTotalTime += elapsed.count();

			// Measure heap sort execution time (average over 10 runs)
			vector<int> heapSortedArray = array;
			start = chrono::high_resolution_clock::now();
			make_heap(heapSortedArray.begin(), heapSortedArray.end());
			sort_heap(heapSortedArray.begin(), heapSortedArray.end());
			end = chrono::high_resolution_clock::now();
			elapsed = end - start;
			heapSortTotalTime += elapsed.count();

			// Measure quick sort execution time (average over 10 runs)
			vector<int> quickSortedArray = array;
			start = chrono::high_resolution_clock::now();
			sort(quickSortedArray.begin(), quickSortedArray.end());
			end = chrono::high_resolution_clock::now();
			elapsed = end - start;
			quickSortTotalTime += elapsed.count();

			// Measure stable sort execution time (average over 10 runs)
			vector<int> stableQuickSortedArray = array;
			start = chrono::high_resolution_clock::now();
			stable_sort(stableQuickSortedArray.begin(), stableQuickSortedArray.end());
			end = chrono::high_resolution_clock::now();
			elapsed = end - start;
			stableSortTotalTime += elapsed.count();
		}

		cout << "Array size: " << size << endl;
		cout << "Bit Branching Sort Average Execution time (of " << RETRY_COUNT_FOR_AVERAGE << " attempts): " << (bitBranchingSortTotalTime * 1000 / 10.0) << " ms" << endl;
		cout << "LSD Radix Sort Average Execution time (of " << RETRY_COUNT_FOR_AVERAGE << " attempts): " << (lsdRadixSortTotalTime * 1000 / 10.0) << " ms" << endl;
		cout << "Heap Sort Average Execution time (of " << RETRY_COUNT_FOR_AVERAGE << " attempts): " << (heapSortTotalTime * 1000 / 10.0) << " ms" << endl;
		cout << "Quick Sort Average Execution time (of " << RETRY_COUNT_FOR_AVERAGE << " attempts): " << (quickSortTotalTime * 1000 / 10.0) << " ms" << endl;
		cout << "Stable Sort Average Execution time (of " << RETRY_COUNT_FOR_AVERAGE << " attempts): " << (stableSortTotalTime * 1000 / 10.0) << " ms" << endl;
		cout << endl;
	}

	return 0;
}
