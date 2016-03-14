#pragma once

#include <memory>
#include <iostream>
#include <fstream>
#include <cstring>

std::shared_ptr<std::istream> openFileOrPipeInput(std::string fileName);
std::shared_ptr<std::ostream> openFileOrPipeOutput(std::string fileName);
