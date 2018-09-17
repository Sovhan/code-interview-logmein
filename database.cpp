/*!
 * \file Database.cpp
 * \brief implments methods for a simple database, and auxilary data structures.
 * \author Olivier Soldano
 */

// std
#include <unordered_map>
#include <map>
#include <set>
#include <string>
#include <mutex>
#include <unistd.h>

//local
#include "database.h"

// type shrinks
struct DataItem;
struct Instruction;
struct Transaction;
using DataStore = std::map< std::string, DataItem >; // needs to be sorted for mutex accesses (RBtree logN complexity)
using TransStore = std::unordered_map< std::string, Transaction >; // O(1) complexity
using ScopeLock = std::lock_guard< std::mutex >;

/*!
 * \brief enumaration of mutating instructions possible to send to the database
 */
enum MutatingInstructionTypes {
	Put,
	Erase,
};

/*
 * \brief struct representing the string stocked in data Store, and its locks
 */

struct DataItem {
	std::string value;
	std::mutex read_mutex {};
	std::mutex write_mutex {};
	bool alive = true;         //! boolean representing the fact that DataItem is on the way to be destroyed, no mutex should be taken if false (avoid Undifined Behaviour of destroying held mutexes)
	DataItem(): value(""){}
	DataItem(std::string value): value(value) {}
};

/*!
 * \brief struct representing an instruction forged during a transaction to send to the database
 */
struct Instruction {
	std::string key;
	std::string* initial_value;
	std::string* final_value;
	MutatingInstructionTypes instruction_type;
};

/*!
 * \brief struc representin a transaction
 */
struct Transaction {
	std::map< std::string, Instruction > instructions {}; // needs to be ordered the same way as DataStore for mutex access
	std::mutex trans_lock {};
	bool alive = true; //similar to DataItem mechanism
	Transaction(decltype(nullptr)) {}
};

/*!
 * \brief struct representing the database and current transactions
 */
struct Database::DatabasePrivate {
	explicit DatabasePrivate();
	~DatabasePrivate();
	inline bool dataHasKey(std::string& key);
	inline bool keyIsAlive(std::string& key);
	inline bool transactionsHasId(std::string& transaction_id);
	inline bool transactionHasKey(std::string& transaction_id, std::string& key);
	inline bool transactionIsAlive(std::string& transaction_id);

	std::unique_ptr<DataStore> data;
	std::unique_ptr<TransStore> transactions;
};

Database::DatabasePrivate::DatabasePrivate():
	data(std::make_unique<DataStore>()),
	transactions(std::make_unique<TransStore>())
{}

Database::DatabasePrivate::~DatabasePrivate()
{
	delete data.release();
	delete transactions.release();
}

// Helpers //
/*
 * Check if key is an item in the database
 */
inline bool
Database::DatabasePrivate::dataHasKey(std::string& key)
{
	return data->find(key) != data->end();
}

/*
 * Check if key is an item in the database and is alive
 */
inline bool
Database::DatabasePrivate::keyIsAlive(std::string& key)
{
	return dataHasKey(key) && data->at(key).alive;
}

/*
 * check if transaction_id is present among the occuring transactions
 */
inline bool
Database::DatabasePrivate::transactionsHasId(std::string& transaction_id)
{
	return transactions->find(transaction_id) != transactions->end();
}

/*
 * Check if key as already been modified by the transaction at transaction_id
 */
inline bool
Database::DatabasePrivate::transactionHasKey(std::string& transaction_id, std::string& key)
{
	return transactions->at(transaction_id).instructions.find(key)
		!= transactions->at(transaction_id).instructions.end();
}

/*
 * Check if key is an item in the database and is alive
 */
inline bool
Database::DatabasePrivate::transactionIsAlive(std::string& transaction_id)
{
	return transactionsHasId(transaction_id) && transactions->at(transaction_id).alive;
}
// END Helpers //

Database::Database():
	d_ptr(std::make_unique<Database::DatabasePrivate>())
{}

Database::~Database()
{
	delete d_ptr.release();
}

void
Database::put(std::string key, std::string value)
{
	if ( d_ptr->dataHasKey(key) ) {
		if ( not d_ptr->keyIsAlive(key) ) {
			throw std::runtime_error("Put failed on key '" + key + "': zombie key");
		}

		std::lock(d_ptr->data->at(key).write_mutex, d_ptr->data->at(key).read_mutex);
		ScopeLock lock_w(d_ptr->data->at(key).write_mutex, std::adopt_lock);
		ScopeLock lock_r(d_ptr->data->at(key).read_mutex, std::adopt_lock);
		d_ptr->data->at(key).value.assign(value);
	} else {
		d_ptr->data->emplace(key, value);
	}

	ScopeLock lock(d_ptr->data->at(key).write_mutex);
	if ( d_ptr->data->at(key).value != value )
		throw std::runtime_error("Put failed on key '" + key + "': could not complete");
}

void
Database::put(std::string key, std::string value, std::string transaction_id)
{
	if (d_ptr->transactionIsAlive(transaction_id) ) {

		ScopeLock lock(d_ptr->transactions->at(transaction_id).trans_lock);
		if ( d_ptr->transactionHasKey(transaction_id, key) ) {
        //key has already been given an instruction during transaction, update final_value and instruction type
			d_ptr->transactions->at(transaction_id).instructions.at(key).final_value->assign(value);
			d_ptr->transactions->at(transaction_id).instructions
				.at(key).instruction_type = MutatingInstructionTypes::Put;
		} else {
		// construct new instruction
			Instruction inst = { key,
								 nullptr, // nullptr signifies no prior value
								 new std::string(value),
								 MutatingInstructionTypes::Put };

			if ( d_ptr->dataHasKey(key) ) {
				inst.initial_value = new std::string(d_ptr->data->at(key).value);
			}

			d_ptr->transactions->at(transaction_id).instructions.emplace(key, inst);
		}
	} else {
		throw std::runtime_error("No existing transaction with name: " + transaction_id);
	}
}

std::string*
Database::get(std::string key)
{
	if ( d_ptr->keyIsAlive(key) ) {
		ScopeLock lock(d_ptr->data->at(key).read_mutex);
		return new std::string(d_ptr->data->at(key).value);
	}

	return nullptr;
}

std::string*
Database::get(std::string key, std::string transaction_id)
{
	if ( d_ptr->transactionIsAlive(transaction_id) ) {
		ScopeLock lock(d_ptr->transactions->at(transaction_id).trans_lock);
		if ( d_ptr->transactionHasKey(transaction_id, key) ) {
			auto result_ptr = d_ptr->transactions->at(transaction_id).instructions.at(key).final_value;
			return (result_ptr != nullptr) ? new std::string( *result_ptr ) : result_ptr;
		}
	} else {
		throw std::runtime_error("Cannot get" + key
									 + " from transaction '" + transaction_id
									 + "': transaction not  existing");
	}

	// if key not yet modified by the transaction, return its current value in data store
	return get(key);
}

void
Database::erase(std::string key)
{
	if ( d_ptr->keyIsAlive(key) ) {
		std::lock(d_ptr->data->at(key).write_mutex, d_ptr->data->at(key).read_mutex);
		ScopeLock lock_w(d_ptr->data->at(key).write_mutex, std::adopt_lock);
		ScopeLock lock_r(d_ptr->data->at(key).read_mutex, std::adopt_lock);
		d_ptr->data->erase(key);
	}
}

void
Database::erase(std::string key, std::string transaction_id)
{
	if ( d_ptr->transactionsHasId(transaction_id) ){

		if ( d_ptr->transactionHasKey(transaction_id, key) ) {
			d_ptr->transactions->at(transaction_id).instructions.at(key)
				.instruction_type = MutatingInstructionTypes::Erase;
		}
	}
}

void
Database::createTransaction(std::string transaction_id)
{
	if ( d_ptr->transactions->find(transaction_id) == d_ptr->transactions->end() ) {
		d_ptr->transactions->emplace(transaction_id, nullptr);
	} else {
		throw std::runtime_error("Transaction with name: " + transaction_id + " already exists");
	}
}

void
Database::rollbackTransaction(std::string transaction_id)
{
	if ( d_ptr->transactions->find(transaction_id) == d_ptr->transactions->end() ) {
		throw std::runtime_error("No transaction " + transaction_id + " to rollback");
	} else {
		{
		ScopeLock lock(d_ptr->transactions->at(transaction_id).trans_lock);
		d_ptr->transactions->at(transaction_id).alive = false;
		}
		d_ptr->transactions->erase(transaction_id);
	}
}

void
Database::commitTransaction(std::string transaction_id)
{
	if ( not d_ptr->transactionIsAlive(transaction_id) ) {
		throw std::runtime_error("Cannot commit transaction '" + transaction_id
									 + "': transaction not existing");
	}

	ScopeLock lock(d_ptr->transactions->at(transaction_id).trans_lock);

	if ( not d_ptr->transactionIsAlive(transaction_id) ) {
		// we were concurrently executing the transaction commit with an opération that deleted the transaction;
		// do nothing and return
		return;
	}

	// first lock all mutexes in order (so has to lock only the needed ressources and avoid deadlocks)
	for ( auto key_instruction_init = d_ptr->transactions->at(transaction_id).instructions.begin();
		  key_instruction_init != d_ptr->transactions->at(transaction_id).instructions.end();
		  ++key_instruction_init) {
		try {
			if ( d_ptr->keyIsAlive(key_instruction_init->second.key) ) {
				d_ptr->data->at(key_instruction_init->second.key).write_mutex.lock();
			}
		} catch (const std::out_of_range& oor) {
			rollbackTransaction(transaction_id);
			throw std::runtime_error("Commit of transaction '" + transaction_id + "' failed");
		}
	}


	auto key_instruction_exec = d_ptr->transactions->at(transaction_id).instructions.begin();
	std::string* last_key = new std::string(key_instruction_exec->second.key);
	// execute instructions
	for ( ; key_instruction_exec != d_ptr->transactions->at(transaction_id).instructions.end();
		  ++key_instruction_exec) {
		auto inst = key_instruction_exec->second;
		auto initial_value = inst.initial_value;

		// if DataItem has been modified whatsoever since beginning of transaction, fail the commit
		if ( (initial_value == nullptr && d_ptr->keyIsAlive(inst.key)) // was not created when transaction started but now is
			 || (initial_value != nullptr && not d_ptr->keyIsAlive(inst.key)) // or was alive and now isn't
			 || (initial_value != nullptr //or has been tampered
				 && d_ptr->keyIsAlive(inst.key)
				 && d_ptr->data->at(inst.key).value != *initial_value) ) {
			break; // get out of execution loop
		}

		switch (inst.instruction_type){
		case MutatingInstructionTypes::Put:
			if ( d_ptr->dataHasKey(inst.key) ) {
				ScopeLock lock(d_ptr->data->at(inst.key).read_mutex);
				d_ptr->data->at(inst.key).value.assign(*inst.final_value);
			} else {
				d_ptr->data->emplace(inst.key, *inst.final_value);
			}
			break;
		case MutatingInstructionTypes::Erase:
		    {
			ScopeLock lock(d_ptr->data->at(inst.key).read_mutex);
			d_ptr->data->at(inst.key).alive = false;
		    }
			break;
		}
		last_key->assign(inst.key);
	}

	// release all mutexes and effectivelly erase dead DataItems
	auto key_instruction_release = d_ptr->transactions->at(transaction_id).instructions.rbegin();
	// first step bas from end to where we stopped (most of the time no step)
	while ( key_instruction_release->second.key != *last_key) {
			++key_instruction_release;
	}
	// Then release aquired mutexes, and effectively delete DataItems.
	for ( ; key_instruction_release != d_ptr->transactions->at(transaction_id).instructions.rend();
		  ++key_instruction_release) {

		d_ptr->data->at(key_instruction_release->second.key).write_mutex.unlock();

		if ( key_instruction_release->second.instruction_type == MutatingInstructionTypes::Erase ) {
			d_ptr->data->erase(key_instruction_release->second.key);
		}
	}

	// if exec loop not over report failure
	if ( key_instruction_exec != d_ptr->transactions->at(transaction_id).instructions.end() ) {
		d_ptr->transactions->erase(transaction_id);
		throw std::runtime_error("Transaction '" + transaction_id
								 + "' commits on tampered data: transaction aborted");
	}

	d_ptr->transactions->erase(transaction_id);
}
