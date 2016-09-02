#ifndef PRIORITY_QUEUE_GUARD
#define PRIORITY_QUEUE_GUARD

#include <cassert>
#include <cstddef>
#include <exception>
#include <iostream>
#include <set>
#include <memory>
#include <utility>

// wyjątki
struct PriorityQueueNotFoundException : public std::exception
{
	virtual const char* what() const throw()
	{
		return "Key doesn't exist in queue";
	}
} PQNotFoundEx;

struct PriorityQueueEmptyException : public std::exception
{
	virtual const char* what() const throw()
	{
		return "Invalid operation on empty queue";
	}
} PQEmptyEx;


template<typename K, typename V>
class PriorityQueue
{
	static_assert(std::is_nothrow_destructible<K>::value, "Key type must be no-throw destructible!");
	static_assert(std::is_nothrow_destructible<V>::value, "Value type must be no-throw destructible!");

private: // comparators

	typedef std::pair<std::shared_ptr<K>, std::shared_ptr<V>> key_value_pair;

	// po kluczu
	struct CompByFst
	{
		bool operator()(const key_value_pair& a, const key_value_pair& b)
		{
			if (!(*a.first == *b.first))
				return *a.first < *b.first;
			return *a.second < *b.second;
		}
	};

	// po wartości
	struct CompBySnd
	{
		bool operator()(const key_value_pair& a, const key_value_pair& b)
		{
			if (!(*a.second == *b.second))
				return *a.second < *b.second;
			return *a.first < *b.first;
		}
	};

public: // public typedefs

	typedef size_t size_type;
	typedef K key_type;
	typedef V value_type;

private: // members and helpers

	// czyszczenie kolejki, no throw
	void clear() noexcept
	{
		PriorityQueue<K, V> tmp;
		swap(tmp);
	}

	// kolejka jest w stanie invalid, kiedy była ofiarą move ctor-a
	bool isValid() const
	{
		return pairs_by_K != nullptr && pairs_by_V != nullptr;
	}

	typedef std::multiset<key_value_pair, CompByFst> pairs_by_key_set;
	typedef std::multiset<key_value_pair, CompBySnd> pairs_by_value_set;

	// kontenery uporządkowane po kluczu i po wartości
	std::unique_ptr<pairs_by_key_set> pairs_by_K;
	std::unique_ptr<pairs_by_value_set> pairs_by_V;
	
	// zmienne pomocnicze dla operacji changeValue
	typename pairs_by_key_set::iterator inserted_K;
	typename pairs_by_value_set::iterator inserted_V;

public: // interface


	// Konstruktor bezparametrowy tworzący pustą kolejkę
	// zlozonosc: O(1)
	PriorityQueue() :
			pairs_by_K(new std::multiset<key_value_pair, CompByFst>()),
			pairs_by_V(new std::multiset<key_value_pair, CompBySnd>())
	{ }

	// Konstruktor kopiujący
	// zlozonosc: O(queue.size())
	PriorityQueue(const PriorityQueue<K, V>& queue) :
			pairs_by_K(new std::multiset<key_value_pair, CompByFst>(*queue.pairs_by_K)),
			pairs_by_V(new std::multiset<key_value_pair, CompBySnd>(*queue.pairs_by_V))
	{ }

	// Konstruktor przenoszący
	// nothrow, zlozonosc: O(queue.size())
	PriorityQueue(PriorityQueue<K, V>&& queue) noexcept
	{
		*this = std::move(queue);
		//patrz komentarz w move=, queue moze byc popsute
	}

	// strong guarantee, zlozonosc: O(queue.size()), copy-and-swap idiom
	PriorityQueue<K, V>& operator=(const PriorityQueue<K, V>& queue)
	{
		if(this != &queue)
		{
			PriorityQueue<K, V> temp(queue);
			swap(temp);
		}
		return *this;
	}

	// nothrow, zlozonosc: O(1)
	PriorityQueue<K, V>& operator=(PriorityQueue<K, V>&& queue) noexcept
	{
		pairs_by_K = std::move(queue.pairs_by_K);
		pairs_by_V = std::move(queue.pairs_by_V);

		//po wykonaniu move w queue sa pointer'y, ktore sa == nullptr
		//zatem queue jest w dosc paskudnym stanie, dlatego przy wywolaniu metody odwolujacej sie do pol obiektu
		//nalezy brac pod uwage mozliwosc, ze ta kolejka jest w stanie niepoprawnym
		return *this;
	}


	// Metoda zwracająca liczbę par (klucz, wartość) przechowywanych w kolejce
	// zlozonosc: O(1)
	size_type size() const noexcept
	{
		if (!isValid())
			return 0;
		assert(pairs_by_K->size() == pairs_by_V->size());
		return pairs_by_K->size();
	}

	// Metoda zwracająca true wtedy i tylko wtedy, gdy kolejka jest pusta
	// zlozonosc: O(1)
	bool empty() const noexcept
	{
		return size() == 0;
	}

	// Metoda wstawiająca do kolejki parę o kluczu key i wartości value
	// strong guarentee, zlozonosc: O(log size())
	void insert(const K& key, const V& value)
	{
		//kopia argumentow insert'a
		auto to_find = std::make_pair(std::make_shared<K>(key), std::make_shared<V>(value));
		auto pair_iterator = pairs_by_K->find(to_find); //iterator do istniejacej juz pary == to_find
		
		if (pair_iterator != pairs_by_K->end()) //to_find jest juz w multisecie
		{
			//wkladamy kopie pary shared_ptr'ów bedacych juz w multisecie
			inserted_K = pairs_by_K->insert(*pair_iterator); //wyjatek w tym miejscu nie zmienia stanu kolejki

			try
			{
				inserted_V = pairs_by_V->insert(*pair_iterator);
			}
			catch(...)
			{
				//w razie niepowodzenia przy 2. insertcie, usuwamy 1. element
				//erase jest no-throw
				pairs_by_K->erase(inserted_K); 
				throw;
			}

		}
		else //nie ma w secie
		{
			//wstawiamy nową parę shared_ptr'ów
			inserted_K = pairs_by_K->insert(to_find);
			try
			{
				inserted_V = pairs_by_V->insert(to_find);
			}
			catch(...)
			{
				//w razie niepowodzenia usuwamy 1. element
				pairs_by_K->erase(inserted_K); 
				throw;
			}
		}
	}

	// Metoda zwracajaca najmniejsza wartosc w kolejce
	// strong guarantee, zlozonosc: O(1)
	const V& minValue() const
	{		
		if (empty())
			throw PQEmptyEx;
		return *(pairs_by_V->begin()->second);
	}

	// Metoda zwracajaca najwieksza wartosc w kolejce
	// strong guarantee, zlozonosc: O(1)
	const V& maxValue() const
	{
		if (empty())
			throw PQEmptyEx;
		return *(pairs_by_V->rbegin()->second);
	}

	// Metoda zwracająca klucz przypisany do najmniejszej wartości
	// strong guarantee, zlozonosc: O(1)
	const K& minKey() const
	{
		if (empty())
			throw PQEmptyEx;
		return *(pairs_by_V->begin()->first);
	}

	// Metoda zwracająca klucz przypisany do najwiekszej wartości
	// strong guarantee, zlozonosc: O(1)
	const K& maxKey() const
	{
		if (empty())
			throw PQEmptyEx;
		return *(pairs_by_V->rbegin()->first);
	}

	// Metoda usuwająca z kolejki jedną parę o najmniejszej wartosci
	// strong guarantee, zlozonosc: O(log size())
	void deleteMin()
	{
		if (empty())
			return;
		auto to_delete1 = pairs_by_V->begin();
		auto to_delete2 = pairs_by_K->find(*to_delete1); //except do tego momentu nic nie robi

		pairs_by_V->erase(to_delete1); //nothrow
		pairs_by_K->erase(to_delete2); //nothrow
	}

	// Metoda usuwająca z kolejki jedną parę o najwiekszej wartosci
	// strong guarantee, zlozonosc: O(log size())
	void deleteMax()
	{
		if (empty())
			return;
		auto to_delete1 = pairs_by_V->end();
		to_delete1--;
		auto to_delete2 = pairs_by_K->find(*to_delete1);

		pairs_by_V->erase(to_delete1); //nothrow
		pairs_by_K->erase(to_delete2); //nothrow
	}

	// Metoda zmieniająca dotychczasową wartość przypisaną kluczowi key na nową
	// strong guarantee, zlozonosc: O(log size())
	void changeValue(const K& key, const V& value)
	{
		//szukamy pary o danym kluczu
		auto to_find = std::make_pair(std::make_shared<K>(key), std::make_shared<V>(minValue()));
		auto pair_iterator = pairs_by_K->lower_bound(to_find);
		
		if (pair_iterator == pairs_by_K->end() || !(*pair_iterator->first == key))
			throw PQNotFoundEx;
			
		//zapisujemy parę którą chcemy usunąć
		auto pair_to_delete = *pair_iterator;
		
		//wstawiamy nową parę
		//insert zapisuje nam iteratory do wstawionych elementów
		//może pojawić się wyjątek, ale mamy strong guarantee
		insert(key, value);
		
		//szukamy znowu pary do usunięcia
		typename pairs_by_key_set::iterator to_delete1;
		typename pairs_by_value_set::iterator to_delete2;
		try
		{
			to_delete1 = pairs_by_K->find(pair_to_delete);
			to_delete2 = pairs_by_V->find(pair_to_delete);
		}
		catch(...)
		{			
			//jeśli pojawił się wyjątek przy szukaniu, usuwamy to co wstawiliśmy
			//mamy zapisane iteratory z inserta
			//erase jest no-throw
			pairs_by_K->erase(inserted_K);
			pairs_by_V->erase(inserted_V);	
			throw;			
		}
		
		//jeśli wszystko poszło dobrze, usuwamy starą parę (no-throw)
		pairs_by_K->erase(to_delete1);
		pairs_by_V->erase(to_delete2);
	}

	// Metoda scalająca zawartość kolejki z podaną kolejką queue; ta operacja usuwa
	// wszystkie elementy z kolejki queue i wstawia je do kolejki *this
	// strong guarantee
	// tworzenie kopii zapasowej kolejki na wypadek wyjatku rzuconego przy mergowaniu: O(size())
	// kopiowanie multisetów queue do *this: O(queue.size() * log (queue.size() + size()))
	// zlozonosc: O(size() + queue.size() * log (queue.size() + size()))
	void merge(PriorityQueue<K, V>& queue)
	{
		if (this != &queue)
		{
			PriorityQueue<K, V> tmp(*this); //kopia zapasowa
			try {
				pairs_by_K->insert(queue.pairs_by_K->begin(), queue.pairs_by_K->end());
				pairs_by_V->insert(queue.pairs_by_V->begin(), queue.pairs_by_V->end());
			}
			catch (...) {
				swap(tmp);
				throw;
			}
			queue.clear();
		}
	}


	// Metoda zamieniającą zawartość kolejki z podaną kolejką queue
	// no-throw guarantee, zlozonosc: O(1)
	void swap(PriorityQueue<K, V>& queue) noexcept
	{
		if (this != &queue)
		{
			std::swap(pairs_by_K, queue.pairs_by_K);
			std::swap(pairs_by_V, queue.pairs_by_V);
		}
	}

	template<typename X, typename Y>
	friend void swap(const PriorityQueue<X, Y>& lhs, const PriorityQueue<X, Y>& rhs) noexcept;

	template<typename X, typename Y>
	friend bool operator==(const PriorityQueue<X, Y>& lhs, const PriorityQueue<X, Y>& rhs);

	template<typename X, typename Y>
	friend bool operator<(const PriorityQueue<X, Y>& lhs, const PriorityQueue<X, Y>& rhs);
};

// no-throw guarantee, zlozonosc: O(1)
template<typename X, typename Y>
void swap(PriorityQueue<X, Y>& lhs, PriorityQueue<X, Y>& rhs) noexcept
{
	lhs.swap(rhs);
}

// wszystkie operatory mają strong guarantee
template<typename X, typename Y>
bool operator==(const PriorityQueue<X, Y>& lhs, const PriorityQueue<X, Y>& rhs)
{
	// jeśli kolejki mają różną wielkość, zwracamy false
	if (lhs.size() == rhs.size())
	{
		//jesli obie kolejki sa puste, zwracamy true
		if(lhs.size() == 0)
			return true;

		auto it1 = lhs.pairs_by_K->begin();
		auto it2 = rhs.pairs_by_K->begin();
		auto end1 = lhs.pairs_by_K->end();

		// jeśli na którejś pozycji kolejki się różnią, zwracamy false
		for (; it1 != end1; it1++, it2++)
		{
			if (!(*(it1->first) == *(it2->first) && //po kluczu
				*(it1->second) == *(it2->second))) //po wartości
				return false;
		}

		return true;
	}
	return false;
}

template<typename X, typename Y>
bool operator<(const PriorityQueue<X, Y>& lhs, const PriorityQueue<X, Y>& rhs)
{
	if (lhs.size() == 0 && rhs.size() == 0)
		return false;
	if (lhs.size() == 0 || rhs.size() == 0)
		return lhs.size() == 0;

	//w tym momencie obie kolejki sa niepuste
	auto it1 = lhs.pairs_by_K->begin();
	auto it2 = rhs.pairs_by_K->begin();
	auto end1 = lhs.pairs_by_K->end();
	auto end2 = rhs.pairs_by_K->end();

	assert(it1 != end1 && it2 != end2);

	while (it1 != end1 && it2 != end2)
	{
		if (!(*(it1->first) == *(it2->first))) //po kluczu
			return *(it1->first) < *(it2->first);
		if (!(*(it1->second) == *(it2->second))) //po wartosci
			return *(it1->second) < *(it2->second);

		++it1;
		++it2;
	}
	//skonczyla sie ktoras z kolejek
	//zwracam true jesli skonczyla sie tylko pierwsza kolejka, wpp. false
	//poniewaz albo sa rowne, albo pierwsza jest dluzsza, w obu przypadkach pierwsza nie jest mniejsza.
	return (it1 == end1 && it2 != end2);
}

// trywialne operatory korzystające z poprzednich
template<typename X, typename Y>
bool operator!=(const PriorityQueue<X, Y>& lhs, const PriorityQueue<X, Y>& rhs)
{
	return !(lhs == rhs);
}

template<typename X, typename Y>
bool operator>(const PriorityQueue<X, Y>& lhs, const PriorityQueue<X, Y>& rhs)
{
	return !(lhs == rhs) && !(lhs < rhs);
}

template<typename X, typename Y>
bool operator<=(const PriorityQueue<X, Y>& lhs, const PriorityQueue<X, Y>& rhs)
{
	return lhs == rhs || lhs < rhs;
}

template<typename X, typename Y>
bool operator>=(const PriorityQueue<X, Y>& lhs, const PriorityQueue<X, Y>& rhs)
{
	return !(lhs < rhs);
}


#endif

