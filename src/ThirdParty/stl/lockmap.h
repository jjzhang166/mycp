// lockmap.h file here
#ifndef __lockmap_h__
#define __lockmap_h__

// 
#include <vector>
#include <map>
#include "stldef.h"
#include <algorithm>
#include <boost/thread/mutex.hpp>

#ifndef AUTO_LOCK
#define AUTO_LOCK(l) boost::mutex::scoped_lock lock(l.mutex())
#endif // AUTO_LOCK

template<typename K, typename T>
class CLockMap
	: public std::map<K, T>
{
public:
	typedef std::pair<K, T>		Pair;
protected:
	boost::mutex m_mutex;
public:
	boost::mutex & mutex(void) {return m_mutex;}
	const boost::mutex & mutex(void) const {return m_mutex;}

	void insert(const K& k, const T& t)
	{
		AUTO_LOCK((*this));
		std::map<K, T>::insert(Pair(k, t));
	}
	bool find(const K& k, T & out, bool erase)
	{
		AUTO_LOCK((*this));
		typename std::map<K, T>::iterator iter = std::map<K, T>::find(k);
		if (iter == std::map<K, T>::end())
			return false;
		out = iter->second;
		if (erase)
			std::map<K, T>::erase(iter);
		return true;
	}
	bool find(const K& k, T & out) const
	{
		boost::mutex::scoped_lock lock(const_cast<boost::mutex&>(m_mutex));
		typename std::map<K, T>::const_iterator iter = std::map<K, T>::find(k);
		if (iter == std::map<K, T>::end())
			return false;
		out = iter->second;
		return true;
	}
	bool exist(const K& k) const
	{
		boost::mutex::scoped_lock lock(const_cast<boost::mutex&>(m_mutex));
		typename std::map<K, T>::const_iterator iter = std::map<K, T>::find(k);
		return iter != std::map<K, T>::end();
	}

	bool remove(const K& k)
	{
		AUTO_LOCK((*this));
		typename std::map<K, T>::iterator iter = std::map<K, T>::find(k);
		if (iter != std::map<K, T>::end())
		{
			std::map<K, T>::erase(iter);
			return true;
		}
		return false;
	}

	void clear(bool is_lock = true)
	{
		if (is_lock)
		{
			AUTO_LOCK((*this));
			std::map<K, T>::clear();
		}else
		{
			std::map<K, T>::clear();
		}
	}

public:
	CLockMap(void) {}
	virtual ~CLockMap(void)
	{
		clear();
	}
};

template<typename K, typename T>
class CLockMapPtr
	: public CLockMap<K, T>
{
public:
	T find(const K& k, bool erase)
	{
		T out = 0;
		CLockMap<K, T>::find(k, out, erase);
		return out;
	}
	const T find(const K& k) const
	{
		T out = 0;
		CLockMap<K, T>::find(k, out);
		return out;
	}
	bool remove(const K& k)
	{
		T t = find(k, true);
		if (t != 0)
			delete t;
		return t != 0;
	}
	void clear(bool is_lock = true, bool is_delete = true)
	{
		if (is_lock)
		{
			AUTO_LOCK((*this));
			if (is_delete)
				for_each(CLockMap<K, T>::begin(), CLockMap<K, T>::end(), DeletePair());
			std::map<K, T>::clear();
		}else
		{
			if (is_delete)
				for_each(CLockMap<K, T>::begin(), CLockMap<K, T>::end(), DeletePair());
			std::map<K, T>::clear();
		}
	}

public:
	CLockMapPtr(void) {}
	virtual ~CLockMapPtr(void)
	{
		clear();
	}
};

template<typename K, typename T>
class CLockMultiMap
	: public std::multimap<K, T>
{
public:
	typedef std::pair<K, T> Pair;
	typedef typename std::multimap<K, T>::iterator IT;
	typedef typename std::multimap<K, T>::const_iterator CIT;
	typedef typename std::pair<CIT, CIT> Range;

protected:
	boost::mutex m_mutex;

public:
	boost::mutex & mutex(void) {return m_mutex;}
	const boost::mutex & mutex(void) const {return m_mutex;}

	void insert(const K& k, const T& t, bool clear = false)
	{
		AUTO_LOCK((*this));
		if (clear)
			std::multimap<K, T>::erase(k);
		std::multimap<K, T>::insert(Pair(k, t));
	}
	bool find(const K& k, T & out, bool erase)
	{
		AUTO_LOCK((*this));
		IT iter = std::multimap<K, T>::find(k);
		if (iter == std::multimap<K, T>::end())
			return false;
		out = iter->second;
		if (erase)
			std::multimap<K, T>::erase(iter);
		return true;
	}
	bool find(const K& k, std::vector<T> & out, bool erase)
	{
		bool result = false;
		AUTO_LOCK((*this));
		Range range = std::multimap<K, T>::equal_range(k);
		for(CIT iter=range.first; iter!=range.second; ++iter)
		{
			result = true;
			out.push_back(iter->second);
		}
		if (result && erase)
			std::multimap<K, T>::erase(k);
		return result;
	}
	bool find(const K& k, T & out) const
	{
		boost::mutex::scoped_lock lock(const_cast<boost::mutex&>(m_mutex));
		CIT iter = std::multimap<K, T>::find(k);
		if (iter == std::multimap<K, T>::end())
			return false;
		out = iter->second;
		return true;
	}
	bool find(const K& k, std::vector<T> & out) const
	{
		bool result = false;
		boost::mutex::scoped_lock lock(const_cast<boost::mutex&>(m_mutex));
		Range range = std::multimap<K, T>::equal_range(k);
		for(CIT iter=range.first; iter!=range.second; ++iter)
		{
			result = true;
			out.push_back(iter->second);
		}
		return result;
	}
	size_t sizek(const K& k) const
	{
		size_t result = 0;
		boost::mutex::scoped_lock lock(const_cast<boost::mutex&>(m_mutex));
		Range range = std::multimap<K, T>::equal_range(k);
		for(CIT iter=range.first; iter!=range.second; ++iter)
		{
			result++;
		}
		return result;
	}
	bool exist(const K& k) const
	{
		boost::mutex::scoped_lock lock(const_cast<boost::mutex&>(m_mutex));
		CIT iter = std::multimap<K, T>::find(k);
		return iter != std::multimap<K, T>::end();
	}

	void remove(const K& k)
	{
		boost::mutex::scoped_lock lock(m_mutex);
		std::multimap<K, T>::erase(k);
	}

	void clear(bool is_lock = true)
	{
		if (is_lock)
		{
			AUTO_LOCK((*this));
			std::multimap<K, T>::clear();
		}else
		{
			std::multimap<K, T>::clear();
		}
	}

public:
	CLockMultiMap(void) {}
	virtual ~CLockMultiMap(void)
	{
		clear();
	}
};

#endif // __lockmap_h__

