/*
* For convenience, this file contains both the definition for Bit Branching Trees and the code for
* benchmarking their performance. The below configurations can be used to change test parameters.
* This file was tested on MSC and GCC compilers.
*
* To benchmark against other structures, add the below to main() under other similar blocks:
* auto structureTotalTime = measure( // Update the name of structureTotalTime as you see fit
*	insertionArray, // Leave this as is
*	[/* Add a reference to the structure here *\/](int value) { /* Insert the given value into the structure here *\/ },
* 	[]() { /* Traverse the structure in order here *\/ },
* 	[]() { /* Do any assertions on the structure here *\/ },
* 	[/* Add a reference to the structure here *\/](int value) { /* Search for the given value in the structure here *\/ },
* 	[/* Add a reference to the structure here *\/](int value) { /* Erase the given value from the structure here *\/ }
* );
* You can then print the measured time using something similar to the cout statements at the end of main()
*/

#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>
#include <chrono>
#include <random>
#include <set>
#include <map>
#include <unordered_set>
#include <functional>
using namespace std;

/* Test parameters (e.g., array sizes or value range) */
#define STARTING_ORDER_OF_MAGNITUDE 2 // The starting array size's order of magnitude (e.g., 2 will begin at size 100)
#define ENDING_ORDER_OF_MAGNITUDE 7 // The ending array size's order of magnitude (e.g., 7 will begin at size 10,000,000)
#define RETRY_COUNT_FOR_AVERAGE 10 // The number of retries per array size (e.g., 10). An average performance time will be calculated
#define MAX_VALUE 2147483646 // The max range of used numbers (e.g., 10, the lowst possible values is 0, as only positive integers are supported in this implementaion), setting this to 0 uses size/100 as the range
#define INSERT_SORTED false // Whether or not to sort the insertion test data
#define INCLUDE_INSERTION true // Whether or not to include insertion time in the final calculation
#define INCLUDE_FINDING true // Whether or not to include finding time in the final calculation
#define INCLUDE_DELETION true // Whether or not to include erasing time in the final calculation
#define INCLUDE_TRAVERSAL false // Whether or not to include ordered traversal time in the final calculation

#define KEY_SIZE 32 // The key size for integers, used in the below tests. This implementation does not allow changing this constant

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
class bit_branching_tree_node
{
public:
	bit_branching_tree_node(int val) : value(val) {}
	/* Each node allocates as mane pointers as the key size for its branches, though these aren't initialized until needed */
	bit_branching_tree_node* branches[KEY_SIZE];
	/* A bit mask the marks reserved branches, used to test for branches instead of polling the branches array since that array is never fully initialized */
	int reservedPointersBitMask = 0;
	/* The number of occurances of this value, this can be replaced with a singly-linked list for object comparison */
	int count = 1;
	/* The node's value */
	int value;
};


/* The bit branching tree class */
class bit_branching_tree
{
private:
	bit_branching_tree_node* root = nullptr;

	/* Traverses the tree in order, recursively, and appends its values to the given array in order */
	void inOrderTraversal(vector<int>& array, bit_branching_tree_node* node)
	{
		unsigned int unvisitedBranchesTo1sBitMask = node->reservedPointersBitMask & ~node->value;
		unsigned int unvisitedBranchesTo0sBitMask = node->reservedPointersBitMask & node->value;

		// Visits reserved branches leading to zeros the left (i.e., smaller numbers first)
		// These numbers are guranteed to be smaller than this node and are ordered left to right
		while (unvisitedBranchesTo0sBitMask != 0)
		{
			// Uses bit operations to find reserved branches
			unsigned int branchIndex = KEY_SIZE - 1 - countLeadingZeros(unvisitedBranchesTo0sBitMask); // The index of the first reserved branch from the left
			int branchBitMask = 1 << branchIndex; // The bit mask for the above branch
			inOrderTraversal(array, node->branches[branchIndex]); // Visits the child
			unvisitedBranchesTo0sBitMask ^= branchBitMask; // Removes the branch from the remaning
		}

		// Adds self to the ordered array as many times as the value was counted
		for (int i = 0; i < node->count; i++)
		{
			array.push_back(node->value);
		}

		// Visits reserved branches leading to ones from the right (i.e., smaller numbers first)
		// These numbers are guranteed to be larger than this node and are ordered right to left
		while (unvisitedBranchesTo1sBitMask != 0)
		{
			int branchIndex = countTrailingZeros(unvisitedBranchesTo1sBitMask); // The index of the first reserved branch from the right
			int branchBitMask = (1 << (branchIndex)); // The bit mask for the above branch
			inOrderTraversal(array, node->branches[branchIndex]); // Visits the child
			unvisitedBranchesTo1sBitMask ^= branchBitMask; // Removes the branch from the remaning
		}
	}

public:
	/* Inserts a new value into the tree */
	void insert(int value)
	{
		// If the tree has no root, then the new value is inserted as the root and the function completes
		if (!root)
		{
			root = new bit_branching_tree_node(value);
			return;
		}

		// Traces a path through the tree until the new value is inserted
		bit_branching_tree_node* current = root;
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
			bool branchAlreadyExists = (branchingBit & current->reservedPointersBitMask) > 0;

			if (branchAlreadyExists)
			{ // If a node already exists at the distination branch, go there.
				current = current->branches[branchingIndex];
			}
			else
			{ // Otherwise, make a node there, mark it in the reservedBranchesBitMask, and conclude
				current->branches[branchingIndex] = new bit_branching_tree_node(value);
				current->reservedPointersBitMask |= branchingBit;
				return;
			}
		}
	}

	/* Erases a value from the tree */
	bool erase(int value)
	{
		bit_branching_tree_node* current = root;
		bit_branching_tree_node* parent = nullptr;
		unsigned int currentBranchingBit;

		// Traces a path through the tree until the value is found and deleted, or until it certainly isn't 
		while (true)
		{
			// Finds the longest common prefix between the node's value and the input by XORing them and counting the result's leading zeros
			int bitDifference = current->value ^ value;
			int longestCommonPrefixLength = countLeadingZeros(bitDifference);

			if (longestCommonPrefixLength == KEY_SIZE)
			{ // If the value exists, a number of cases can occure
				if (current->count >= 2)
				{ // If the value was counted more than one time, its count is reduced, then the function terminates
					current->count--;
				}
				else if (current->reservedPointersBitMask == 0)
				{ // Otherwise, if the value's node has no children, remove it
					delete current;
					if (parent)
					{ // If the remove node wasn't the root, indicate on its parent that its branch is no longer utilized
						parent->reservedPointersBitMask &= ~currentBranchingBit;
					}
					else
					{ // Otherwise, set the root to null
						root = nullptr;
					}
				}
				else
				{ // Otherwise, if the value's node has children, elect a child to replace it
					// Uses the largest child's data to replace the deleted node, effectivly replacing the deleted node
					unsigned int lastChildIndex = countTrailingZeros(current->reservedPointersBitMask);
					unsigned int lastChildBitMask = 1 << lastChildIndex;
					bit_branching_tree_node* lastChild = current->branches[lastChildIndex];
					current->value = lastChild->value;
					current->count = lastChild->count;

					current->reservedPointersBitMask &= ~lastChildBitMask; // Unresrves that child's branch, as the child replaced that node

					// Moves the replacing node's children with it
					current->reservedPointersBitMask |= lastChild->reservedPointersBitMask;
					while (lastChild->reservedPointersBitMask)
					{
						int subBranchIndex = KEY_SIZE - 1 - countLeadingZeros(lastChild->reservedPointersBitMask);
						int subBranchBitMask = 1 << subBranchIndex;
						current->branches[subBranchIndex] = lastChild->branches[subBranchIndex];
						lastChild->reservedPointersBitMask ^= subBranchBitMask;
					}
					delete lastChild; // Finally, deletes the hallow child
				}

				return true; // Returns true, indicating that a matching node was found and erased
			}

			
			unsigned int branchingIndex = KEY_SIZE - 1 - longestCommonPrefixLength;
			unsigned int branchingBitMask = 1 << branchingIndex;

			// Terminates if the next branch to follow has now node under it, as it is confirmed that the value to erase isn't in the tree
			bool branchAlreadyExists = (branchingBitMask & current->reservedPointersBitMask) > 0;
			if (!branchAlreadyExists) return false;

			// Otherwise, continues to the next iteration using the next node in the path
			parent = current;
			current = current->branches[branchingIndex];
			currentBranchingBit = branchingBitMask;
		}
	}

	/* Checkes whether or not the requested value is in the tree */
	bool find(int value)
	{
		bit_branching_tree_node* current = root;

		// Traces a path through the tree until the value is found, or until it is guranteed not to be in the tree
		while (true)
		{
			// Finds the longest common prefix between the node's value and the input by XORing them and counting the result's leading zeros
			int bitDifference = current->value ^ value;
			int longestCommonPrefixLength = countLeadingZeros(bitDifference);

			// If the prefix length equals the key size, then the input value and node's value match, so a match is reported
			if (longestCommonPrefixLength == KEY_SIZE) return true;

			unsigned int branchingIndex = KEY_SIZE - 1 - longestCommonPrefixLength;
			unsigned int branchingBit = 1 << branchingIndex;

			// Terminates if the next branch to follow has now node under it, as it is confirmed that the value to find isn't in the tree
			bool branchAlreadyExists = (branchingBit & current->reservedPointersBitMask) > 0;
			if (!branchAlreadyExists) return false;

			// Otherwise, continues to the next iteration using the next node in the path
			current = current->branches[branchingIndex];
		}
	}

	/* Returns an array from the tree */
	vector<int> toArray()
	{
		vector<int> array;
		bit_branching_tree_node* node = root;
		inOrderTraversal(array, root);
		return array;
	}
};

/* Selectively measure specifc structure functions using the configurations at the top of the file */
double measure(
	vector<int>& array,
	const function<void(int)>& insert,
	const function<void()>& traverse,
	const function<bool()>& assertions,
	const function<void(int)>& find,
	const function<void(int)>& erase
) {
	double total = 0;

	for (int i = 0; i < RETRY_COUNT_FOR_AVERAGE; ++i) {
		auto start = chrono::high_resolution_clock::now();
		for (int i = 0; i < array.size(); ++i)
		{
			insert(array[i]);
		}
		auto end = chrono::high_resolution_clock::now();
		chrono::duration<double> elapsed = end - start;

		if (INCLUDE_INSERTION)
		{
			total += elapsed.count();
		}

		start = chrono::high_resolution_clock::now();
		traverse();
		end = chrono::high_resolution_clock::now();
		elapsed = end - start;

		if (INCLUDE_TRAVERSAL)
		{
			total += elapsed.count();
		}

		assert(assertions());

		start = chrono::high_resolution_clock::now();
		for (int i = 0; i < array.size(); ++i)
		{
			find(array[i]);
		}
		end = chrono::high_resolution_clock::now();
		elapsed = end - start;

		if (INCLUDE_FINDING)
		{
			total += elapsed.count();
		}

		start = chrono::high_resolution_clock::now();
		for (int i = 0; i < array.size(); ++i)
		{
			erase(array[i]);
		}
		end = chrono::high_resolution_clock::now();
		elapsed = end - start;

		if (INCLUDE_DELETION)
		{
			total += elapsed.count();
		}
	}

	total /= RETRY_COUNT_FOR_AVERAGE;
	double totalInMs = total * 1000;
	return totalInMs;
}

/* Checks whether or not the given array is sorted */
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

int main()
{
	// Repeats the test for the given order of magnitude range
	for (int i = STARTING_ORDER_OF_MAGNITUDE; i < ENDING_ORDER_OF_MAGNITUDE; ++i)
	{
		// Initializes the size based on the order of magnitude for this loop
		int size = 1;
		for (int j = 0; j < i; ++j)
		{
			size *= 10;
		}

		// Inserts random values into the array based on the given size
		vector<int> insertionArray;
		random_device rd;
		mt19937 gen(rd());
		uniform_int_distribution<> dis(0, MAX_VALUE == 0 ? size / 100 : MAX_VALUE);
		for (int k = 0; k < size; ++k)
		{
			insertionArray.push_back(dis(gen));
		}

		// Sorts the array if specifed
		if (INSERT_SORTED)
		{
			sort(insertionArray.begin(), insertionArray.end());
		}

		// Measure bit branching trees performance
		bit_branching_tree bitBranchingTree;
		vector<int> array;
		double bitBranchingTreeTotalTime = measure(
			insertionArray,
			[&bitBranchingTree](int value) { bitBranchingTree.insert(value); },
			[&bitBranchingTree, &array]() { array = bitBranchingTree.toArray(); },
			[&bitBranchingTree, &array, &size]() { return isSorted(array) && array.size() == size; },
			[&bitBranchingTree](int value) { bitBranchingTree.find(value); },
			[&bitBranchingTree](int value) { bitBranchingTree.erase(value); }
		);

		// Measure binary search trees performance
		multiset<int> binaryTree;
		auto binaryTreeTotalTime = measure(
			insertionArray,
			[&binaryTree](int value) { binaryTree.insert(value); },
			[&binaryTree]() {
				vector<int> temp;
				copy(binaryTree.begin(), binaryTree.end(), std::back_inserter(temp));
			},
			[]() { return true; },
			[&binaryTree](int value) { binaryTree.find(value); },
			[&binaryTree](int value) { binaryTree.erase(value); }
		);

		// Measure hash maps performance
		unordered_set<int> hashMap;
		auto hashMapTotalTime = measure(
			insertionArray,
			[&hashMap](int value) { hashMap.insert(value); },
			[]() {},
			[]() { return true; },
			[&hashMap](int value) { hashMap.find(value); },
			[&hashMap](int value) { hashMap.erase(value); }
		);
		
		cout << "Number of operations: " << size << endl;
		cout << "Bit Branching Tree Average Execution time (of " << RETRY_COUNT_FOR_AVERAGE << " attempts): " << bitBranchingTreeTotalTime << " ms" << endl;
		cout << "Binary Search Tree Average Execution time (of " << RETRY_COUNT_FOR_AVERAGE << " attempts): " << binaryTreeTotalTime << " ms" << endl;
		cout << "Hash Map Average Execution time (of " << RETRY_COUNT_FOR_AVERAGE << " attempts): " << hashMapTotalTime << " ms" << endl;
		cout << endl;
	}
	return 0;
}
