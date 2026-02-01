#include "UciClient.h"



int main() {
	zobrist::initialise_zobrist_keys();
	initialise_all_databases();
	bq::Logger::setLevel(bq::LogLevel::trace);
	bq::Logger::logToFile("output.txt", true);
	bq::Logger::stopConsoleLogging();

	bq::UciClient client;
	client.run();
}