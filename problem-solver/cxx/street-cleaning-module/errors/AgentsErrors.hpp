#pragma once

#include <exception>
#include <string>

class StartNodeNotFoundError : public std::exception
{
private:
  std::string msg;

public:
  explicit StartNodeNotFoundError(std::string const & msg) {}

  char const * what()
  {
    return msg.c_str();
  }
};

class VertexDegreeNotFoundError : public std::exception
{
private:
  std::string msg;

public:
  explicit VertexDegreeNotFoundError(std::string const & msg) {}

  char const * what()
  {
    return msg.c_str();
  }
};

class NodeIsNotStreetError : public std::exception
{
private:
  std::string msg;

public:
  explicit NodeIsNotStreetError(std::string const & msg) {}

  char const * what()
  {
    return msg.c_str();
  }
};