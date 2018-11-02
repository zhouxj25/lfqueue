#include "nlqueue.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
using namespace std;

bool allExit(false);

int prod(const string &num, NoLockRing *ring)
{
	FILE *file = fopen(num.c_str(), "w+");
	if (!file)
	{
		cerr << this_thread::get_id() << ", open file: " << num << ", error." << endl;
		return -1;	
	}

	int maxLen(100);
	int size(1000000);
	int total(0);
	for (int i = 0; i < size; ++i)
	{
		int len = random() % maxLen;
		if (len < 10)
			len = 10;
		char *p = (char*)malloc(len + 1);
        if (!p)
        {
            cerr << "prod malloc null" << endl;
            break;
        }
		memset(p, 0, len + 1);
		for (int j = 0; j < len; ++j)
		{
			char c = random() % 26 + 'a';
			*p++ = c;
			fwrite(&c, 1, 1, file);
		}
		p -= len;
		while (ring->push(p) <= 0 )
		{
			//cout << "ring is full, wait!" << total << endl;
			this_thread::sleep_for(chrono::milliseconds(5));
			fflush(file);
		}

		fwrite("\n", 1, 1, file);
		++total;
	}

	fclose(file);
}

int cons(const string &num, NoLockRing *ring)
{
	FILE *file = fopen(num.c_str(), "w+");
	if (!file)
	{
		cerr << this_thread::get_id() << ", open file: " << num << ", error." << endl;
		return -1;	
	}

	char **p = (char**)malloc(40);
    if(!p)
    {
        cerr << "cons malloc null" << endl;
        fclose(file);
        return -1;
    }
	int total(0);
	int nexit(1);
	while (1)
	{
		int ret = ring->pop(5, (void**)p);
		if (ret <= 0)
		{
			this_thread::sleep_for(chrono::milliseconds(5));
			fflush(file);
			if (allExit && (nexit-- <= 0))
			{
				cerr << "ring is empty,exit, total: " << total << endl;
				break;
			}
			continue;
		}

		for (int i =0; i<ret; ++i)
		{
			if (!p[i])
			{
				cerr << i << ", error" << endl;
				continue;
			}
			fwrite(p[i], strlen(p[i]), 1, file);
			fwrite("\n", 1, 1, file);
			free(p[i]);
			++total;
		}

	}

	free(p);
	fclose(file);
}

int main()
{
	NoLockRing *ring = new NoLockRing(1024, RING_MP | RING_MC);
    	if (!ring)
    	{
        	cerr << "new ring error" << endl;
        	return -1;
    	}
	if (ring->init())
	{
		delete ring;
		return -1;
	}
	int num(2);
	thread th[num];
	for (int i = 0; i < num; ++i)
		th[i] = thread(prod, string("a").append(to_string(i)), ring);

	thread con[num];
	for (int i = 0; i < num; ++i)
		con[i] = thread(cons, string("b").append(to_string(i)), ring);
	for (int i = 0; i < num; ++i)
		th[i].join();
	allExit = true;
	for (int i = 0; i < num; ++i)
		con[i].join();
	delete ring;
	return 0;
}
