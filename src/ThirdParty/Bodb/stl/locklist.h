// locklist.h file here
#ifndef __locklist_h__
#define __locklist_h__

// 
#include "list"
#include "stldef.h"
#include <algorithm>
#include <boost/thread/mutex.hpp>

#ifndef AUTO_LOCK
#define AUTO_LOCK(l) boost::mutex::scoped_lock lock(l.mutex())
#endif // AUTO_LOCK

template<typename T>
class CLockList
	: public std::list<T>
{
protected:
	boost::mutex m_mutex;

public:
	boost::mutex & mutex(void) {return m_mutex;}
	const boost::mutex & mutex(void) const {return m_mutex;}

	void add(T t)
	{
		AUTO_LOCK((*this));
		std::list<T>::push_back(t);
	}
	bool front(T & out, bool is_pop = true)
	{
		AUTO_LOCK((*this));
		typename std::list<T>::iterator pIter = std::list<T>::begin();
		if (pIter == std::list<T>::end())
			return false;

		out = *pIter;
		if (is_pop)
			std::list<T>::pop_front();
		return true;
	}


	void clear(bool is_lock = true)
	{
		if (is_lock)
		{
			AUTO_LOCK((*this));
			std::list<T>::clear();
		}else
		{
			std::list<T>::clear();
		}
	}

public:
	CLockList(void)
	{
	}
	~CLockList(void)
	{
		clear();
	}
};

template<typename T>
class CLockListPtr
	: public CLockList<T>
{
public:
	T front(void)
	{
		T result = 0;
		CLockList<T>::front(result);
		return result;
	}

	void clear(bool is_lock = true, bool is_delete = true)
	{
		if (is_lock)
		{
			AUTO_LOCK((*this));
			if (is_delete)
				for_each(std::list<T>::begin(), std::list<T>::end(), DeletePtr());
			std::list<T>::clear();
		}else
		{
			if (is_delete)
				for_each(std::list<T>::begin(), std::list<T>::end(), DeletePtr());
			std::list<T>::clear();
		}
	}
public:
	CLockListPtr(void)
	{}
	~CLockListPtr(void)
	{
		clear();
	}
};

#endif // __locklist_h__
