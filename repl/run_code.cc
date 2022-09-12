#include <iostream>
#include <fstream>
#include <map>
#include <thread>

#include "evaluator.h"
#include "object.h"




void run_code(const std::string& path) {
	std::ifstream fin;
	fin.open(path, std::ios::in);
	autumn::Evaluator evaluator;
	
	std::string code;
	std::string line;
	while (std::getline(fin, line)) {
		code += line;
	}
	
	auto object = evaluator.eval(code);
	std::cout << object->inspect() << std::endl;
}
