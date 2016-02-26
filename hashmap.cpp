#include<iostream>
#include<assert.h>
#include<thread>
#include<mutex>
#define MAX_HASH_SIZE  1000
#define SEGMENT_SIZE 32
#define INVALID_INDEX -1
using namespace std;
int simplehash(long int n);
void threadfunc(void *ptr,int id);
//take care of exceptin handling
//instead of using error codes, use exception


//hopscotch hashing algorithm 
class CHashMap
{
	
	private:
		struct Bucket
		{	
			int key;
			int data;
			bool used;
			//current index in the hash
			int _first_delta;
			// next index in the hash
			int _next_delta;
		};
		struct Segment
		{
			struct Bucket *start_bucket;
			struct Bucket *last_bucket;
			// mutex protects the segment
			std::mutex _lock;
		};
		int arr[MAX_HASH_SIZE];
		struct Bucket buckarray[MAX_HASH_SIZE];
		struct Segment segments[MAX_HASH_SIZE];
		struct Segment& getSegment(int hash);
		struct Bucket* getFreeBucketInCacheLine(struct Segment &segment);
		struct Bucket* getNearestFreeBucket(struct Segment &segment, struct Bucket* bucket);
		struct Bucket* displaceTheFreeBucket(struct Segment &segment, 
							struct Bucket *start_bucket, 
							struct Bucket *free_bucket);
	public:
		CHashMap();
		int add(int key ,int data);
		int remove(int key );
		bool contains(int key);
		int get(int key);
		//void printContent();
	
		
};
CHashMap::CHashMap()
{
	for(int i = 0;i < MAX_HASH_SIZE;i++)
	{
		buckarray[i].used = false;
		buckarray[i]._first_delta = i;
		if(i == MAX_HASH_SIZE-1)
		{
			buckarray[i]._next_delta = -1;
		}
		else	
		{
			buckarray[i]._next_delta = i+1;
		}
		segments[i].start_bucket = &buckarray[i];
		if(i+SEGMENT_SIZE > MAX_HASH_SIZE-1)
		{
			segments[i].last_bucket = &buckarray[MAX_HASH_SIZE-1];
		}
		else
		{
			segments[i].last_bucket = &buckarray[i+SEGMENT_SIZE];
		}
	}
}
CHashMap::Bucket* CHashMap::displaceTheFreeBucket(struct Segment& segment, 
							struct Bucket *start_bucket, 
							struct  Bucket *free_bucket)
{
	//start from bucket just before the free_bucket 
	struct Bucket *temp = free_bucket-(SEGMENT_SIZE-1);
	int free_index = free_bucket->_first_delta;
	bool moved = false;
	while(free_bucket - temp < SEGMENT_SIZE)
	{	
		//key of the temp buck 
		int key = temp->key;
		//where does it belong is decided by hashing it
		int hash = simplehash(key);
		// temp really belongs to index "hash", can we move this key to free bucket location 
		if(free_index - hash  > SEGMENT_SIZE-1)
		{
			//can not move temp to free_bucket. 
			//Free_bucket is too far from the cacheline of temp's original bucket
			temp++;
			continue;
		}
		else
		{	
			//we can move the temp to free_bucket
			free_bucket->used = true;
			free_bucket->key = temp->key;
			free_bucket->data = temp->data;
			//no need to update the _first_delta or _next_delta;  deltas are never updated
			free_bucket = temp;
			moved = true;
			break;
		}
	}
	if(!moved)
	{
		//we could not move free_bucket closer to start_bucket
		return NULL;
	}
	//ok now we could move  free bucket little closer  to the  start_bucket 
	int current_distance = free_bucket - start_bucket;
	if(current_distance > SEGMENT_SIZE -1)
	{
		//we still need to move free_bucket close to the start_bucket
		return displaceTheFreeBucket(segment, start_bucket, free_bucket);
	}
}
CHashMap::Bucket* CHashMap::getNearestFreeBucket(struct Segment& segment, struct Bucket *bucket)
{
	//iterate over the bucket array to find a free bucket. 
	//necessary care is taken to make sure not to cross array boundaries.
	//_first_delta is the current index in buckarray, _next_delta is next index
	struct Bucket *tempbucket = bucket;
	while(tempbucket->_first_delta <MAX_HASH_SIZE)
	{
		if(tempbucket->used == false)	
			return tempbucket;
		tempbucket++;
	}
	return NULL;
}
CHashMap::Bucket* CHashMap::getFreeBucketInCacheLine( struct Segment& segment)
{
	struct Bucket *bucket = segment.start_bucket;
	while(bucket != segment.last_bucket)
	{
		if(bucket->used == false)return bucket;
	}
	return NULL;
}
CHashMap::Segment& CHashMap::getSegment(int hash)
{
	return segments[hash];
}
int CHashMap::add(int key, int data)
{
	assert(key<MAX_HASH_SIZE);
	arr[key] = data;
	return 0;
	// hopscotch segments
	//get the segment and find a bucket to put the key and data
	struct Segment& segment = getSegment(simplehash(key));
	struct Bucket* start_bucket = segment.start_bucket;
	//check if the segment already has the key
	segment._lock.lock();
	if(contains(key))
	{
		segment._lock.unlock();
		return 0;
	}
	
	//if there is a free bucket available  in cacheline use it 
	struct Bucket* free_bucket = getFreeBucketInCacheLine(segment);
	if(free_bucket != NULL)
	{
		free_bucket->used = true;
		free_bucket->key = key;
		free_bucket->data = data;
		return 0;
	}
	//oops couldn't find a free bucket in cacheline
	// need to find an empty bucket and move it close to the cacheline
	free_bucket = getNearestFreeBucket(segment, start_bucket);
	if(free_bucket == NULL)
	{
		//hash is full. Currently we do not resize hash
		return -1;
	}
	int distance_to_free = free_bucket - start_bucket;
	if(distance_to_free > SEGMENT_SIZE-1)	
	{
		//recursively displace close to the startbucket
		displaceTheFreeBucket(segment, start_bucket, free_bucket);
	}
	segment._lock.unlock();
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
	
	//hopscotch
	struct Segment& segment = getSegment(simplehash(key));
	struct Bucket* bucket = segment.start_bucket;
	struct Bucket *last_bucket = segment.last_bucket;
	while(bucket != last_bucket)
	{	
		if(bucket->key == key)
			return true;
		else 
			bucket++;

	}
	return false;
}
int CHashMap::get(int key)
{
	if(contains(key)) return arr[key];
	else return -1;
}

int main()
{
	int count = 2;
	int key;
	int data;
	CHashMap map;
	thread thread1(threadfunc, &map,1);
	thread thread2(threadfunc, &map,2);
	thread1.join();
	thread2.join();
}
int simplehash(long int n)
{
	return n%MAX_HASH_SIZE;
}
void threadfunc(void *ptr,int id)
{
	CHashMap *map = (CHashMap *)ptr;
	int count = 5;	
	while(count){
		int key = rand()%10;
		int data = rand()%10000;
		map->add(key, data);
		cout<<id<<" "<<key<<endl;
		if(!map->contains(key))
		{	
			cout<<"BOOM"<<endl;
		}	
		else
		{
			cout<<"Cool"<<endl;
		}
		count--;
	}
}
