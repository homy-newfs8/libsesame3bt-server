cmake_minimum_required(VERSION 3.16.0)

idf_component_register(
	SRCS
		"lib/libsesame3bt-server/SesameServer.cpp"
	INCLUDE_DIRS
		"lib/libsesame3bt")
