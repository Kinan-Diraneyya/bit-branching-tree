#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>
#include <chrono>
#include <random>
#include <set>
#include <map>
#include <unordered_set>
using namespace std;

/* Test parameters (e.g., array sizes or value range) */
#define STARTING_ORDER_OF_MAGNITUDE 2 // The starting array size's order of magnitude (e.g., 2 will begin at size 100)
#define ENDING_ORDER_OF_MAGNITUDE 7 // The ending array size's order of magnitude (e.g., 7 will begin at size 10,000,000)
#define RETRY_COUNT_FOR_AVERAGE 10 // The number of retries per array size (e.g., 10). An average performance time will be calculated
#define MAX_VALUE 2147483646 // The max range of used numbers (e.g., 10, the lowst possible values is 0, as only positive integers are supported in this implementaion)
#define INSERT_SORTED true // Whether or not to sort the insertion test data
#define INCLUDE_INSERTION true
#define INCLUDE_FINDING false
#define INCLUDE_DELETION true

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
	BitBranchingTreeNode(int val) : value(val) {}
	/* Each node allocates as mane pointers as the key size for its branches, though these aren't initialized until needed */
	BitBranchingTreeNode* branches[KEY_SIZE];
	/* A bit mask the marks reserved branches, used to test for branches instead of polling the branches array since that array is never fully initialized */
	int reservedBranchesBitMask = 0;
	/* The number of occurances of this value, this can be replaced with a singly-linked list for object comparison */
	int count = 1;
	/* The node's value */
	int value;
};

/* The bit branching tree class */
class BitBranchingTree
{
private:
	BitBranchingTreeNode* root = nullptr;

public:
	/* Inserts a new value into the tree */
	void insert(int value)
	{
		// If the tree has no root, then the new value is inserted as the root and the function completes
		if (!root)
		{
			root = new BitBranchingTreeNode(value);
			return;
		}

		// Traces a path through the tree until the new value is inserted
		BitBranchingTreeNode* current = root;
		while (true)
		{
			// Finds the longest common prefix between the node's value and the input by XORing them and counting the result's leading zeros
			unsigned int bitDifference = current->value ^ value;
			unsigned int longestCommonPrefixLength = countLeadingZeros(bitDifference);

			// If the prefix length matches the key size, then a match is found
			if (longestCommonPrefixLength == KEY_SIZE)
			{
				// The count of the matching node is increased instead of inserting a new node
				current->count++;
				return;
			}

			// Creates a bit mask of the branching index and uses it to check whether or not the branch leads to a node
			unsigned int branchingIndex = KEY_SIZE - 1 - longestCommonPrefixLength;
			unsigned int branchingBit = 1 << branchingIndex;
			bool branchAlreadyExists = (branchingBit & current->reservedBranchesBitMask) > 0;

			if (branchAlreadyExists)
			{ // If a node already exists at the distination branch, go there.
				current = current->branches[branchingIndex];
			}
			else
			{ // Otherwise, make a node there, mark it in the reservedBranchesBitMask, and conclude
				current->branches[branchingIndex] = new BitBranchingTreeNode(value);
				current->reservedBranchesBitMask |= branchingBit;
				return;
			}
		}
	}

	bool erase(int value)
	{
		BitBranchingTreeNode* current = root;
		BitBranchingTreeNode* parent = nullptr;
		unsigned int currentBranchingBit;

		while (true)
		{
			int bitDifference = current->value ^ value;
			int matchingBitsCount = countLeadingZeros(bitDifference);

			if (matchingBitsCount == KEY_SIZE)
			{
				if (current->count >= 2)
				{
					current->count--;
				}
				else if (current->reservedBranchesBitMask == 0)
				{
					delete current;
					if (parent)
					{
						parent->reservedBranchesBitMask &= ~currentBranchingBit;
					}
					else
					{
						root = nullptr;
					}
				}
				else
				{
					unsigned int lastChildIndex = countTrailingZeros(current->reservedBranchesBitMask);
					unsigned int lastChildBit = 1 << lastChildIndex;
					BitBranchingTreeNode* lastChild = current->branches[lastChildIndex];
					current->value = lastChild->value;
					current->count = lastChild->count;
					current->reservedBranchesBitMask |= lastChild->reservedBranchesBitMask;
					current->reservedBranchesBitMask &= ~lastChildBit;
					while (lastChild->reservedBranchesBitMask)
					{
						int subBranchIndex = KEY_SIZE - 1 - countLeadingZeros(lastChild->reservedBranchesBitMask);
						int subBranchBit = 1 << subBranchIndex;
						current->branches[subBranchIndex] = lastChild->branches[subBranchIndex];
						lastChild->reservedBranchesBitMask ^= subBranchBit;
					}
					delete lastChild;
				}

				return true;
			}

			unsigned int branchingIndex = KEY_SIZE - 1 - matchingBitsCount;
			unsigned int branchingBit = 1 << branchingIndex;
			bool branchAlreadyExists = (branchingBit & current->reservedBranchesBitMask) > 0;
			bool brancingBitInNewValueIsOne = (value >> branchingIndex) & 1;

			if (!branchAlreadyExists) return false;

			parent = current;
			current = current->branches[branchingIndex];
			currentBranchingBit = branchingBit;
		}
	}

	bool find(int value)
	{
		BitBranchingTreeNode* current = root;

		while (true)
		{
			int bitDifference = current->value ^ value;
			int matchingBitsCount = countLeadingZeros(bitDifference);

			if (matchingBitsCount == KEY_SIZE) return true;

			unsigned int branchingIndex = KEY_SIZE - 1 - matchingBitsCount;
			unsigned int branchingBit = 1 << branchingIndex;
			bool branchAlreadyExists = (branchingBit & current->reservedBranchesBitMask) > 0;
			bool brancingBitInNewValueIsOne = (value >> branchingIndex) & 1;

			if (!branchAlreadyExists) return false;

			current = current->branches[branchingIndex];
		}
	}
};

int main()
{
	for (int i = STARTING_ORDER_OF_MAGNITUDE; i < ENDING_ORDER_OF_MAGNITUDE; ++i)
	{
		double bitBranchingTreeTotalTime = 0.0;
		double binaryTreeTotalTime = 0.0;
		double hashMapTotalTime = 0.0;

		int size = 1;
		for (int j = 0; j < i; ++j)
		{
			size *= 10;
		}

		for (int k = 0; k < RETRY_COUNT_FOR_AVERAGE; ++k) {
			vector<int> insertionArray;
			vector<int> searchArray;
			vector<int> removalArray;
			random_device rd;
			mt19937 gen(rd());
			uniform_int_distribution<> dis(0, size/100);
			for (int j = 0; j < size; ++j)
			{
				if (INCLUDE_INSERTION)
				{
					insertionArray.push_back(dis(gen));
				}
				if (INCLUDE_FINDING)
				{
					searchArray.push_back(dis(gen));
				}
				if (INCLUDE_DELETION)
				{
					removalArray.push_back(dis(gen));
				}
			}

			if (INSERT_SORTED)
			{
				sort(insertionArray.begin(), insertionArray.end());
			}

			BitBranchingTree* bitBranchingTree = new BitBranchingTree();
			for (int j = 0; j < insertionArray.size(); ++j)
			{
				bitBranchingTree->insert(insertionArray[j]);
			}
			// Measure bitBranchingTree execution time (average over 10 runs)
			auto start = chrono::high_resolution_clock::now();
			for (int j = 0; j < searchArray.size(); ++j)
			{
				bitBranchingTree->find(searchArray[j]);
			}
			for (int j = 0; j < removalArray.size(); ++j)
			{
				bitBranchingTree->erase(removalArray[j]);
			}
			auto end = chrono::high_resolution_clock::now();
			chrono::duration<double> elapsed = end - start;
			bitBranchingTreeTotalTime += elapsed.count();

			multiset<int> binaryTree;
			for (int j = 0; j < insertionArray.size(); ++j)
			{
				binaryTree.insert(insertionArray[j]);
			}
			// Measure built-in set (binary search tree) execution time (average over 10 runs)
			start = chrono::high_resolution_clock::now();
			for (int j = 0; j < searchArray.size(); ++j)
			{
				binaryTree.find(searchArray[j]);
			}
			for (int j = 0; j < removalArray.size(); ++j)
			{
				binaryTree.erase(removalArray[j]);
			}
			end = chrono::high_resolution_clock::now();
			elapsed = end - start;
			binaryTreeTotalTime += elapsed.count();

			unordered_set<int> hashMap;
			for (int j = 0; j < insertionArray.size(); ++j)
			{
				hashMap.insert(insertionArray[j]);
			}
			// Measure built-in unordered set (hashmap) execution time (average over 10 runs)
			start = chrono::high_resolution_clock::now();
			for (int j = 0; j < searchArray.size(); ++j)
			{
				hashMap.find(searchArray[j]);
			}
			for (int j = 0; j < removalArray.size(); ++j)
			{
				hashMap.erase(removalArray[j]);
			}
			end = chrono::high_resolution_clock::now();
			elapsed = end - start;
			hashMapTotalTime += elapsed.count();
		}

		cout << "Number of insertion/search/removal operations: " << size << endl;
		cout << "Bit Branching Tree Average Execution time (of " << RETRY_COUNT_FOR_AVERAGE << " attempts): " << (bitBranchingTreeTotalTime * 1000 / 10.0) << " ms" << endl;
		cout << "Binary Search Tree Average Execution time (of " << RETRY_COUNT_FOR_AVERAGE << " attempts): " << (binaryTreeTotalTime * 1000 / 10.0) << " ms" << endl;
		cout << "Hash Map Average Execution time (of " << RETRY_COUNT_FOR_AVERAGE << " attempts): " << (hashMapTotalTime * 1000 / 10.0) << " ms" << endl;
		cout << endl;
	}
	return 0;
}
