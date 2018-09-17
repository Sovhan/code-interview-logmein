/*!
 *\file main.cpp test routine for database API
 *\author Olivier Soldano
 */
#include "database.h"

#include <iostream>
#include <cassert>
#include <thread>

int main()
{
	Database myDb;
	myDb.put("example", "foo");
	assert(*myDb.get("example") == "foo");
	myDb.erase("example");
	assert(myDb.get("example") == nullptr);
	myDb.erase("example");

	myDb.createTransaction("abc");
	try {
		myDb.createTransaction("abc");
	} catch (std::runtime_error e) {
		std::cout << "failed as intedded with " << e.what() << std::endl;
	}
	myDb.put("a", "foo", "abc");
	assert(*myDb.get("a", "abc") == "foo");
	assert(myDb.get("a") == nullptr);
	myDb.createTransaction("xyz");
	myDb.put("a", "bar", "xyz");
	assert(*myDb.get("a", "xyz") == "bar");
	myDb.commitTransaction("xyz");
	assert(*myDb.get("a") == "bar");
	try {
		myDb.commitTransaction("abc");
	} catch (std::runtime_error e) {
		std::cout << "failed as intedded with " << e.what() << std::endl;
	}
	assert(*myDb.get("a") == "bar");
	myDb.createTransaction("abc");
	myDb.put("a", "foo", "abc");
	assert(*myDb.get("a") == "bar");
	myDb.rollbackTransaction("abc");
	try {
		myDb.put("a", "foo", "abc");
	} catch (std::runtime_error e) {
		std::cout << "failed as intedded with " << e.what() << std::endl;
	}
	assert(*myDb.get("a") == "bar");
	myDb.createTransaction("def");
	myDb.put("b", "foo", "def");
	myDb.put("c", "caz", "def");
	myDb.put("d", "ert", "def");
	assert(*myDb.get("a", "def") == "bar");

	//multithread twice
	std::thread t1([&myDb](){
			try {
				myDb.commitTransaction("def");
			} catch (std::runtime_error e) {
				std::cout << "t1 is sad that t2 was faster :( " << e.what() << std::endl;
			};});
	std::thread t2([&myDb](){
			try {
				myDb.commitTransaction("def");
			} catch (std::runtime_error e) {
				std::cout << "t1 is sad that t2 was faster :( " << e.what() << std::endl;
			}
		});
	t1.join();
	t2.join();

	// multithreads concurrents
	myDb.createTransaction("aze");
	myDb.put("b", "fro", "aze");
	myDb.put("c", "crz", "aze");
	myDb.put("d", "ert", "aze");
	myDb.createTransaction("ghj");
	myDb.put("b", "for", "ghj");
	myDb.put("c", "car", "ghj");
	myDb.put("d", "err", "ghj");
	std::thread t3([&myDb](){
			try {
				myDb.commitTransaction("aze");
			} catch (std::runtime_error e) {
				std::cout << "t3 is sad that t4 was faster :( " << e.what() << std::endl;
			}
		});
	std::thread t4([&myDb](){
			try {
				myDb.commitTransaction("ghj");
			} catch (std::runtime_error e) {
				std::cout << "t4 is sad that t3 was faster :( " << e.what() << std::endl;
			}
		});
	t3.join();
	t4.join();
	assert( ( *myDb.get("b") == "fro" && *myDb.get("c") == "crz" && *myDb.get("d") == "ert" )
			|| (*myDb.get("b") == "for" && *myDb.get("c") == "car" && *myDb.get("d") == "err"));
}
