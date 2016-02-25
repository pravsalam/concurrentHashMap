#include<iostream>
#include<assert.h>
#include<thread>
#define MAX_HASH_SIZE  1000
using namespace std;
int simplehash(long int n);
void threadfunc(void *ptr);
//take care of exceptin handling
//instead of using error codes, use exception
class CHashMap
{
	private:
		int arr[MAX_HASH_SIZE];
	public:
		int add(int key ,int data);
		int remove(int key );
		bool contains(int key);
};
int CHashMap::add(int key, int data)
{
	assert(key<MAX_HASH_SIZE);
	arr[key] = data;
	return 0;
}
int CHashMap::remove(int key)
{
	assert(key<MAX_HASH_SIZE);
	arr[key] = -1;
	return 0;
}
bool CHashMap::contains(int key)
{
	assert(key<MAX_HASH_SIZE);
	if(arr[key] != -1)
		return true;
	else return false;
}

int main()
{
	int count = 2;
	int key;
	int data;
	CHashMap map;
	
}
int simplehash(long int n)
{
	return n%MAX_HASH_SIZE;
}
void threadfunc(void *ptr)
{
	CHashmap *map = (*CHashMap)ptr;
	int count = 5;	
	while(count){
		int key = rand()%10;
		int data = rand()%10000;
		ptr->add(simplehash(key), data);
		if(!ptr->contains(simplehash(key)))
		{	
			cout<<"BOOM"<<endl;
		}
		count--;
	}
}
