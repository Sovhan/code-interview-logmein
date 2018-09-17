/*!
 * \file Database.h
 * \brief define an API for a simple database
 * \author Olivier Soldano
*/
#ifndef DATABASE_API_H
#define DATABASE_API_H

#include <memory>

class Database {

public:
	explicit Database();
	~Database();

	/*!
	 *\brief Set the variable “key” to the provided “value”
	 * Throws an exception or error on failure
	 */
	void put(std::string key, std::string value);

	/*!
	 *\brief Set the variable “key” to the provided “value” within the transaction with ID “transaction_id”
	 * Throws an exception or error on failure
	*/
	void put(std::string key, std::string value, std::string transaction_id);

	/*!
	 *\brief gets a pointer to a copy of the value associated with “key” within the data store
	 * Throws an exception or error on failure
	 */

	std::string* get(std::string key);

    /*!
	 * gets a pointer to a copy of the value associated with “key” within the transaction with ID “transaction_id”
	 *Throws an exception or error on failure
	 *\returns null if "key" isn't present in database, a pointer to a std::string otherwise
	 */
	std::string* get(std::string key, std::string transaction_id);

    /*!
	 * Remove the value associated with “key”
	 * Throws an exception or error on failure
	 * No-op if key is absent (in the given example myDb.delete(“example”); myDb.get(“example”) // returns null; myDb.delete(“example”) the last delete does not entail failure)
	 */
	void erase(std::string key);

	/*!
	 * Remove the value associated with “key” within the transaction with ID “transaction_id”
	 * Throws an exception or error on failure
	 * No-op if key is absent (idem as delete, as no example furnished)
	 */
	void erase(std::string key, std::string transaction_id);

    /*!
	 * Starts a transaction with the specified ID. The ID must not be an active transaction ID.
	 * Throws an exception or error on failure
	 */
	void createTransaction(std::string transaction_id);

	/*!
	 * Aborts the transaction and invalidates the transaction with the specified transaction ID
     * Throws an exception or error on failure
	 */
	void rollbackTransaction(std::string transaction_id);

	/*!
	 *\brief Commits the transaction and invalidates its ID.
	 * If there is a conflict (meaning the transaction attempts to change
	 * a value for a key that was mutated after the transaction was created),
	 * the transaction always fails with an exception or an error.
	 */
	void commitTransaction(std::string transaction_id);


private:
	struct DatabasePrivate;
	std::unique_ptr<DatabasePrivate> d_ptr;
};

#endif
