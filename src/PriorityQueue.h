#ifndef PRIORITYQUEUE_H
#define PRIORITYQUEUE_H

#include <queue>
#include <vector>

template <class T, class T2>
class PriorityQueue : public std::priority_queue<T, std::vector<T>, T2> 
{
	public:
	std::vector<T>& GetContainer()
	{
		return this->c;
	}
};

#endif
