#include<iostream>
#include<assert.h>
#include<thread>
#include<mutex>
#include<atomic>
#define MAX_HASH_SIZE  10
#define SEGMENT_SIZE 3
#define INVALID_INDEX -1
using namespace std;
int simplehash(long int n);
void threadfunc(void *ptr,int id);
//take care of exceptin handling
//instead of using error codes, use exception

// pending tasks
// 1. resizing the hash,
// 2. is Array right DS? Should we use some other DS for easy resizing
// 3. improving remove functionality with cachealignment improvement



//hopscotch hashing algorithm 
class CHashMap
{
	
	private:
		struct Bucket
		{	
			int key;
			int data;
			bool used;
			std::atomic<bool> atomic_used;
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
		//int arr[MAX_HASH_SIZE];
		struct Bucket buckarray[MAX_HASH_SIZE];
		struct Segment segments[MAX_HASH_SIZE];
		struct Segment& getSegment(int hash);
		struct Bucket* getBucket(int hash);
		struct Bucket* getFreeBucketInCacheLine(const struct Segment& segment, struct Bucket* const start_bucket);
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
		void printContent();
	
		
};
CHashMap::CHashMap()
{
	for(int i = 0;i < MAX_HASH_SIZE;i++)
	{
		buckarray[i].used = false;
		buckarray[i].atomic_used.store(false);
		buckarray[i]._first_delta = i;
		if(i == MAX_HASH_SIZE-1)
		{
			buckarray[i]._next_delta = -1;
		}
		else	
		{
			buckarray[i]._next_delta = i+1;
		}
		buckarray[i].key = -1;
		buckarray[i].data = -1;
		segments[i].start_bucket = &buckarray[i];
		if(i+SEGMENT_SIZE > MAX_HASH_SIZE-1)
		{
			segments[i].last_bucket = &buckarray[MAX_HASH_SIZE-1];
		}
		else
		{
			segments[i].last_bucket = &buckarray[i+SEGMENT_SIZE-1];
		}
	}
}
CHashMap::Bucket* CHashMap::displaceTheFreeBucket(struct Segment& segment, 
							struct Bucket *start_bucket, 
							struct  Bucket *free_bucket)
{
	cout<<"displace the free bucket called"<<endl;
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
			int tempkey = free_bucket->key;
			int tempdata = free_bucket->data;
			free_bucket->used = true;
			free_bucket->key = temp->key;
			free_bucket->data = temp->data;
			//no need to update the _first_delta or _next_delta;  deltas are never updated
			free_bucket = temp;
			free_bucket->key = tempkey;
			free_bucket->data = tempdata;
			
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
		cout<<"inside displace the free bucket"<<endl;
		return displaceTheFreeBucket(segment, start_bucket, free_bucket);
	}
}
CHashMap::Bucket* CHashMap::getNearestFreeBucket(struct Segment& segment, struct Bucket *bucket)
{
	//iterate over the bucket array to find a free bucket. 
	//necessary care is taken to make sure not to cross array boundaries.
	//_first_delta is the current index in buckarray, _next_delta is next index
	cout<<"temp bucket called"<<endl;
	struct Bucket *tempbucket = bucket;
	bool expected  = false;
	while(tempbucket->_first_delta <MAX_HASH_SIZE)
	{
		cout<<"index = "<< tempbucket - buckarray<<" before atomic set value ="<<tempbucket->atomic_used.load()<<endl;
		if(tempbucket->atomic_used.compare_exchange_strong(expected, true,memory_order_release, memory_order_relaxed))
		{
			cout<<"atomic set check "<<tempbucket->atomic_used.load()<<endl;
			return tempbucket;
		}
		else if(tempbucket->atomic_used.load() ==false)
		{
			cout<<"failed spuriously "<<tempbucket->atomic_used.load()<<endl;
		}
		//if(tempbucket->used == false)	return tempbucket;
		tempbucket++;
		expected = false;
	}
	return NULL;
}
CHashMap::Bucket* CHashMap::getFreeBucketInCacheLine( const Segment& segment,  struct Bucket* const start_bucket)
{
	struct Bucket *bucket = start_bucket;
	//cout<<"start bucket index "<<bucket->_first_delta<<endl;
	bool expected = false;
	while(bucket <= segment.last_bucket)
	{
		cout<<"start bucket index "<<bucket->_first_delta<<" used "<<bucket->atomic_used.load()<<endl;
		if(bucket->atomic_used.compare_exchange_strong(expected, true,memory_order_release, memory_order_relaxed))
			return bucket;
		else if(bucket->atomic_used.load() == false)
		{
			cout<<"failed spuriously"<<endl;
		}
		//if(bucket->used == false) return bucket;
		bucket++;
		expected = false;
	}
	return NULL;
}
CHashMap::Segment& CHashMap::getSegment(int hash)
{
	return segments[hash];
}
CHashMap::Bucket* CHashMap::getBucket(int hash)
{
	return &buckarray[hash];
}
int CHashMap::add(int key, int data)
{
	// hopscotch segments
	//get the segment and find a bucket to put the key and data
	//cout<<"inside add key ="<<key<<" data "<<data<<endl;
	struct Segment& segment = getSegment(simplehash(key));
	struct Bucket* start_bucket = getBucket(simplehash(key));

	//check if the segment already has the key
	segment._lock.lock();
	if(contains(key))
	{
		cout<<"key already present"<<endl;
		segment._lock.unlock();
		return 0;
	}
	
	//if there is a free bucket available  in cacheline use it 
	struct Bucket* free_bucket = getFreeBucketInCacheLine(segment, start_bucket);
	if(free_bucket != NULL)
	{
		cout<<"found a free bucket in cacheline"<<endl;
		free_bucket->used = true;
		free_bucket->key = key;
		free_bucket->data = data;
		segment._lock.unlock();
		cout<<" added key and data"<<endl;
		return 0;
	}
	cout<<"no free bucket in cacheline"<<endl;
	//oops couldn't find a free bucket in cacheline
	// need to find an empty bucket and move it close to the cacheline
	free_bucket = getNearestFreeBucket(segment, start_bucket);
	if(free_bucket == NULL)
	{
		//hash is full. Currently we do not resize hash
		cout<<"could not find a free bucked"<<endl;
		segment._lock.unlock();
		return -1;
	}
	free_bucket->key = key;
	free_bucket->used = true;
	free_bucket->data = data;
	int distance_to_free = free_bucket - start_bucket;
	cout<<"distance to free bucket "<< distance_to_free<<endl;
	if(distance_to_free > SEGMENT_SIZE-1)	
	{
		//recursively displace close to the startbucket
		cout<<"free bucket is too far"<<endl;
		free_bucket = displaceTheFreeBucket(segment, start_bucket, free_bucket);
	}
	cout<<"done add"<<endl;
	segment._lock.unlock();
}
int CHashMap::remove(int key)
{
	//remove 
	int hash = simplehash(key);
	struct Segment& segment = getSegment(hash);
	//search all buckets in the segment that has the key passed in. 
	segment._lock.lock();
	struct Bucket* bucket = segment.start_bucket;
	while(bucket != segment.last_bucket)
	{
		if(bucket->key == key)
		{
			//free the bucket
			bucket->used  = false;
			segment._lock.unlock();
			//cache alignment need to be implemented
			return bucket->data;
		}
		bucket++;
	}
	segment._lock.unlock();
	return -1;
}
bool CHashMap::contains(int key)
{
	//hopscotch
	struct Segment& segment = getSegment(simplehash(key));
	struct Bucket* bucket = segment.start_bucket;
	struct Bucket *last_bucket = segment.last_bucket;
	while(bucket <= last_bucket)
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
	int hash = simplehash(key);
	struct Segment& segment = getSegment(hash);
	struct Bucket *bucket = segment.start_bucket;
	while(bucket <= segment.last_bucket){
		if(bucket->key == key)
			return bucket->data;
		bucket++;
	}
	return -1;
}
void CHashMap::printContent()
{
	cout<<"=================================================="<<endl;
	cout<<"Content of the hash table"<<endl;
	cout<<"=================================================="<<endl;
	
	for(int i =0;i < MAX_HASH_SIZE;i++)
	{
		cout<<i<<"   "<<buckarray[i].key<<"         "<<buckarray[i].data<<endl;
	}	
}

int main()
{
	int count = 2;
	int key;
	int data;
	CHashMap map;
	thread thread1(threadfunc, &map,1);
	//thread thread2(threadfunc, &map,2);
	thread1.join();
	//thread2.join();
	map.printContent();
}
int simplehash(long int n)
{
	return n%MAX_HASH_SIZE;
}
void threadfunc(void *ptr,int id)
{
	CHashMap *map = (CHashMap *)ptr;
	int count = 10;	
	while(count){
		int key = rand()%20;
		int data = rand()%100;
		map->add(key, data);
		cout<<id<<"    "<<key<<"   "<<data<<endl;
		count--;
	}
}
