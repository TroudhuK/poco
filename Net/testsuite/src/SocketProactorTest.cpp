//
// SocketProactorTest.cpp
//
// Copyright (c) 2005-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SocketProactorTest.h"
#include "UDPEchoServer.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/DatagramSocket.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Timestamp.h"
#include <iostream>

using Poco::Net::SocketProactor;
using Poco::Net::StreamSocket;
using Poco::Net::DatagramSocket;
using Poco::Net::ServerSocket;
using Poco::Net::SocketAddress;
using Poco::Thread;
using Poco::Timestamp;


SocketProactorTest::SocketProactorTest(const std::string& name): CppUnit::TestCase(name)
{
}


SocketProactorTest::~SocketProactorTest()
{
}


void SocketProactorTest::testTCPSocketProactor()
{
	EchoServer echoServer;
	SocketProactor proactor(false);
	StreamSocket s;
	s.connect(SocketAddress("127.0.0.1", echoServer.port()));
	int mode = SocketProactor::POLL_READ | SocketProactor::POLL_WRITE | SocketProactor::POLL_ERROR;
	proactor.addSocket(s, mode);
	std::string hello = "hello proactor world";
	bool sent = false, sendPassed = false;
	auto onSendCompletion = [&](std::error_code err, int bytes)
	{
		sendPassed = (err.value() == 0) && (bytes == hello.length());
		sent = true;
	};
	proactor.addSend(s, SocketProactor::Buffer(hello.begin(), hello.end()), onSendCompletion);
	SocketProactor::Buffer buf(0, hello.size());
	bool received = false, receivePassed = false;
	auto onRecvCompletion = [&](std::error_code err, int bytes)
	{
		receivePassed = (err.value() == 0) &&
						(bytes == hello.length()) &&
						(std::string(buf.begin(), buf.end()) == hello);
		received = true;
	};
	proactor.addReceive(s, buf, onRecvCompletion);
	while (!received) proactor.poll();

	assertTrue (sent);
	assertTrue (sendPassed);
	assertTrue (received);
	assertTrue (receivePassed);

	buf.clear();
	buf.resize(hello.size());
	assertFalse(std::string(buf.begin(), buf.end()) == hello);

	sent = false;
	sendPassed = false;
	received = false;
	receivePassed = false;

	proactor.addSend(s, SocketProactor::Buffer(hello.begin(), hello.end()), nullptr);
	proactor.addReceive(s, buf, nullptr);
	int handled = 0, handledTot = 0;
	do
	{
		proactor.poll(&handled);
		handledTot += handled;
	}
	while (handledTot < 2);

	assertTrue(std::string(buf.begin(), buf.end()) == hello);
	assertFalse (sent);
	assertFalse (sendPassed);
	assertFalse (received);
	assertFalse (receivePassed);

	bool error = false;
	bool errorPassed = false;
	auto onError = [&](std::error_code err, int bytes)
	{
		errorPassed = (err.value() != 0) && (bytes == 0);
		error = true;
	};

	StreamSocket errSock(SocketAddress::IPv4);
	proactor.addSocket(errSock, SocketProactor::POLL_ERROR);
	proactor.addSend(errSock, SocketProactor::Buffer(hello.begin(), hello.end()), onError);
	errSock.connectNB(SocketAddress("127.0.0.1", 0xFFEE));
	Thread::sleep(100);
	while (!error)
		proactor.poll();
	assertTrue (error);
	assertTrue(errorPassed);
}


void SocketProactorTest::testUDPSocketProactor()
{
	UDPEchoServer echoServer;
	DatagramSocket s(SocketAddress::IPv4);
	SocketProactor proactor(false);
	int mode = SocketProactor::POLL_READ | SocketProactor::POLL_WRITE;
	proactor.addSocket(s, mode);
	std::string hello = "hello proactor world";
	bool sent = false, sendPassed = false;
	auto onSendCompletion = [&](std::error_code err, int bytes)
	{
		sendPassed = (err.value() == 0) &&
			(bytes == hello.length());
		sent = true;
	};
	proactor.addSendTo(s,
		SocketProactor::Buffer(hello.begin(), hello.end()),
		SocketAddress("127.0.0.1", echoServer.port()),
		onSendCompletion);
	Poco::Net::SocketProactor::Buffer buf(0, hello.size());
	bool received = false, receivePassed = false;
	SocketAddress sa;
	auto onRecvCompletion = [&](std::error_code err, int bytes)
	{
		receivePassed = (err.value() == 0) &&
			(bytes == hello.length()) &&
			(sa.host().toString() == "127.0.0.1") &&
			(sa.port() == echoServer.port()) &&
			(std::string(buf.begin(), buf.end()) == hello);
		received = true;
	};
	proactor.addReceiveFrom(s, buf, sa, onRecvCompletion);
	while (!received) proactor.poll();

	assertTrue (sent);
	assertTrue (sendPassed);
	assertTrue (received);
	assertTrue (receivePassed);

	buf.clear();
	buf.resize(hello.size());
	assertFalse(std::string(buf.begin(), buf.end()) == hello);

	sent = false;
	sendPassed = false;
	received = false;
	receivePassed = false;

	proactor.addSendTo(s,
		SocketProactor::Buffer(hello.begin(), hello.end()),
		SocketAddress("127.0.0.1", echoServer.port()),
		nullptr);
	proactor.addReceiveFrom(s, buf, sa, nullptr);
	int handled = 0, handledTot = 0;
	do
	{
		proactor.poll(&handled);
		handledTot += handled;
	} while (handledTot < 2);

	assertTrue(std::string(buf.begin(), buf.end()) == hello);
	assertFalse (sent);
	assertFalse (sendPassed);
	assertFalse (received);
	assertFalse (receivePassed);
}


void SocketProactorTest::testSocketProactorStartStop()
{
	UDPEchoServer echoServer;
	DatagramSocket s(SocketAddress::IPv4);
	SocketProactor proactor(false);
	int mode = SocketProactor::POLL_READ | SocketProactor::POLL_WRITE;
	proactor.addSocket(s, mode);
	std::string hello = "hello proactor world";
	bool sent = false, sendPassed = false;
	auto onSendCompletion = [&](std::error_code err, int bytes)
	{
		sendPassed = (err.value() == 0) &&
			(bytes == hello.length());
		sent = true;
	};
	proactor.addSendTo(s,
		SocketProactor::Buffer(hello.begin(), hello.end()),
		SocketAddress("127.0.0.1", echoServer.port()),
		onSendCompletion);
	Poco::Net::SocketProactor::Buffer buf(0, hello.size());
	bool received = false, receivePassed = false;
	SocketAddress sa;
	auto onRecvCompletion = [&](std::error_code err, int bytes)
	{
		receivePassed = (err.value() == 0) &&
			(bytes == hello.length()) &&
			(sa.host().toString() == "127.0.0.1") &&
			(sa.port() == echoServer.port()) &&
			(std::string(buf.begin(), buf.end()) == hello);
		received = true;
		proactor.stop();
	};
	proactor.addReceiveFrom(s, buf, sa, onRecvCompletion);
	proactor.run();

	assertTrue (sent);
	assertTrue (sendPassed);
	assertTrue (received);
	assertTrue (receivePassed);

	buf.clear();
	buf.resize(hello.size());
	assertFalse(std::string(buf.begin(), buf.end()) == hello);

	sent = false;
	sendPassed = false;
	received = false;
	receivePassed = false;

	assertFalse (sent);
	assertFalse (sendPassed);
	assertFalse (received);
	assertFalse (receivePassed);

	proactor.addSendTo(s,
		SocketProactor::Buffer(hello.begin(), hello.end()),
		SocketAddress("127.0.0.1", echoServer.port()),
		onSendCompletion);
	proactor.addReceiveFrom(s, buf, sa, onRecvCompletion);
	proactor.run();

	assertTrue(std::string(buf.begin(), buf.end()) == hello);
	assertTrue (sent);
	assertTrue (sendPassed);
	assertTrue (received);
	assertTrue (receivePassed);
}


void SocketProactorTest::testWork()
{
	SocketProactor proactor;
	int executed = 0;
	SocketProactor::Work work = [&]() { ++executed; };
	proactor.addWork(work);
	assertTrue (proactor.runOne() == 1);
	assertEquals (executed, 1);
	assertTrue (proactor.poll() == 0);
	assertEquals (executed, 1);

	UDPEchoServer echoServer;
	DatagramSocket s(SocketAddress::IPv4);
	int mode = SocketProactor::POLL_READ | SocketProactor::POLL_WRITE;
	proactor.addSocket(s, mode);
	proactor.addSendTo(s,
		SocketProactor::Buffer(1, 0),
		SocketAddress("127.0.0.1", echoServer.port()),
		nullptr);
	assertTrue (proactor.poll() == 1);
	assertEquals (executed, 2);
}


void SocketProactorTest::testTimedWork()
{
	SocketProactor proactor;
	int executed = 0;
	SocketProactor::Work work = [&]() { ++executed; };
	proactor.addWork(work, 0);
	proactor.addWork(work, 500);
	assertTrue (proactor.poll() == 1);
	assertEquals (executed, 1);
	Thread::sleep(500);
	assertTrue (proactor.poll() == 1);
	assertEquals (executed, 2);
}


void SocketProactorTest::setUp()
{
}


void SocketProactorTest::tearDown()
{
}


CppUnit::Test* SocketProactorTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SocketProactorTest");

	CppUnit_addTest(pSuite, SocketProactorTest, testTCPSocketProactor);
	CppUnit_addTest(pSuite, SocketProactorTest, testUDPSocketProactor);
	CppUnit_addTest(pSuite, SocketProactorTest, testSocketProactorStartStop);
	CppUnit_addTest(pSuite, SocketProactorTest, testWork);
	CppUnit_addTest(pSuite, SocketProactorTest, testTimedWork);

	return pSuite;
}