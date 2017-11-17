#include "status.h"

#include <cerrno>
#include <cstring>

Status::Status(): code_(kOk), message_("OK") {}

Status::Status(Code code, std::string message) :
    code_(code), message_(std::move(message)) {}

Status Status::Err() {
  return Status(kErr, std::string(strerror(errno)));
}

Status Status::Err(std::string message) {
  return Status(kErr, message);
}

Status Status::Eof() {
  return Status(kEof, "");
}

Status Status::Ok() {
  return Status();
}

bool Status::ok() {
  return code_ == kOk;
}

bool Status::err() {
  return code_ == kErr;
}

bool Status::eof() {
  return code_ == kEof;
}

Status::Code Status::code() {
  return code_;
}

std::string &Status::message() {
  return message_;
}

std::string Status::ToString() {
  return message_;
}
