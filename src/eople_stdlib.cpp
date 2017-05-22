#include "eople_core.h"
#include "eople_stdlib.h"
#include "eople_vm.h"

#include <curl/curl.h>

#include <iostream>
#include <cstdio>
#include <thread>
#include <sstream>

namespace Eople
{
namespace Instruction
{

bool PrintI( process_t process_ref )
{
  Object* result = process_ref->OperandA();
  std::cout << result->int_val << std::endl;

  return true;
}

bool PrintF( process_t process_ref )
{
  Object* result = process_ref->OperandA();
  std::cout << result->float_val << std::endl;

  return true;
}

bool PrintS( process_t process_ref )
{
  Object* text = process_ref->OperandA();
  std::cout << *text->string_ref << std::endl;

  process_ref->TryCollectTempString(text);

  return true;
}

bool PrintSPromise( process_t process_ref )
{
  promise_t promise = process_ref->OperandA()->promise;

  std::cout << *promise->value.string_ref << std::endl;
  return true;
}

bool PrintSArr( process_t process_ref )
{
  auto array_ref = process_ref->OperandA()->array_ref;
  assert(array_ref);

  std::cout << "[";

  for( size_t i = 0; i < array_ref->size(); ++i )
  {
    auto element = (*array_ref)[i];

    if( i != 0 )
    {
      std::cout << ", ";
    }
    std::cout << "\'" << *element.string_ref << "\'";
  }
  std::cout << "]" << std::endl;

  return true;
}

bool PrintFArr( process_t process_ref )
{
  auto array_ref = process_ref->OperandA()->array_ref;
  assert(array_ref);

  std::cout << "[";

  for( size_t i = 0; i < array_ref->size(); ++i )
  {
    auto element = (*array_ref)[i];

    if( i != 0 )
    {
      std::cout << ", ";
    }
    std::cout << element.float_val;
  }
  std::cout << "]" << std::endl;

  return true;
}

bool PrintIArr( process_t process_ref )
{
  auto array_ref = process_ref->OperandA()->array_ref;
  assert(array_ref);

  std::cout << "[";

  for( size_t i = 0; i < array_ref->size(); ++i )
  {
    auto element = (*array_ref)[i];

    if( i != 0 )
    {
      std::cout << ", ";
    }
    std::cout << element.int_val;
  }
  std::cout << "]" << std::endl;

  return true;
}

bool PrintDict( process_t process_ref )
{
  auto dict_ref = process_ref->OperandA()->dict_ref;
  assert(dict_ref);

  std::cout << "{";

  size_t i = 0;
  for( auto it : *dict_ref )
  {
    if( i != 0 )
    {
      std::cout << ", ";
    }
    std::cout << it.first << ":";
    if( it.second.object_type == (u8)ValueType::STRING )
    {
      std::cout << "\"" << *it.second.string_ref << "\"";
    }
    else if( it.second.object_type == (u8)ValueType::INT )
    {
      std::cout << it.second.int_val;
    }
    else if( it.second.object_type == (u8)ValueType::FLOAT )
    {
      std::cout << it.second.float_val;
    }

    ++i;
  }
  std::cout << "}" << std::endl;

  return true;
}

bool PrintObject( process_t process_ref )
{
  auto obj_ref = process_ref->OperandA();
  assert(obj_ref);

  std::ostringstream string_stream;
  if( obj_ref->object_type == (u8)ValueType::STRING )
  {
    string_stream << *obj_ref->string_ref;
  }
  else if( obj_ref->object_type == (u8)ValueType::INT )
  {
    string_stream << obj_ref->int_val;
  }
  else if( obj_ref->object_type == (u8)ValueType::FLOAT )
  {
    string_stream << obj_ref->float_val;
  }

  std::cout << string_stream.str() << std::endl;

  return true;
}

bool GetLine( process_t process_ref )
{
  string_t &new_string = process_ref->CCallReturnVal()->string_ref;
  new_string = new std::string();

  std::getline(std::cin, *new_string);

  return true;
}

bool ArrayConstructor( process_t process_ref )
{
  process_ref->CCallReturnVal()->array_ref = new std::vector<Object>();

  return true;
}

bool ArrayPush( process_t process_ref )
{
  auto array_ref = process_ref->OperandA()->array_ref;
  Object* object = process_ref->OperandB();

  array_ref->push_back(*object);

  return true;
}

bool ArrayPushArray( process_t process_ref )
{
  auto array_ref = process_ref->OperandA()->array_ref;
  Object* object = process_ref->OperandB();

  // push a copy
  array_ref->push_back(Object::BuildArray(*object->array_ref));

  return true;
}

bool ArrayPushString( process_t process_ref )
{
  auto array_ref = process_ref->OperandA()->array_ref;
  Object* object = process_ref->OperandB();

  // push a copy
  string_t string_value = new std::string(*object->string_ref);
  array_ref->push_back(Object::BuildString(string_value));

  return true;
}

bool ArraySize( process_t process_ref )
{
  process_ref->CCallReturnVal()->int_val = process_ref->OperandA()->array_ref->size();

  return true;
}

bool ArrayTop( process_t process_ref )
{
  *process_ref->CCallReturnVal() = process_ref->OperandA()->array_ref->back();

  return true;
}

bool ArrayTopArray( process_t process_ref )
{
  Object &object = process_ref->OperandA()->array_ref->back();

  // return a copy
  *process_ref->CCallReturnVal() = Object::BuildArray(*object.array_ref);

  return true;
}

bool ArrayTopString( process_t process_ref )
{
  Object &object = process_ref->OperandA()->array_ref->back();

  // return a copy
  string_t string_value = new std::string(*object.string_ref);
  *process_ref->CCallReturnVal() = Object::BuildString(string_value);

  return true;
}

bool ArrayPop( process_t process_ref )
{
  process_ref->OperandA()->array_ref->pop_back();

  return true;
}

bool ArrayClear( process_t process_ref )
{
  process_ref->OperandA()->array_ref->clear();

  return true;
}

bool ArraySubscript( process_t process_ref )
{
  auto object = process_ref->OperandA();
  if(object->object_type == (u8)ValueType::ARRAY)
  {
    auto& array_ref = *process_ref->OperandA()->array_ref;
    int_t index = process_ref->OperandB()->int_val;
    auto result = process_ref->OperandC();

    if( index >= array_ref.size() )
    {
      throw std::runtime_error("Array index out of bounds.");
    }
    *result = array_ref[index];
  }
  else if(object->object_type == (u8)ValueType::DICT)
  {
    auto& dict_ref = *process_ref->OperandA()->dict_ref;
    std::string key = *process_ref->OperandB()->string_ref;
    auto result = process_ref->OperandC();

    if( dict_ref.find(key) == dict_ref.end() )
    {
      throw std::runtime_error(std::string("No such key: ") + key);
    }
    *result = dict_ref[key];
  }

  return true;
}

bool Timer( process_t process_ref )
{
  Object* duration = process_ref->OperandA();

  auto future_time = HighResClock::now() + std::chrono::milliseconds(duration->int_val);
  promise_t promise = new Promise(process_ref);
  promise->is_timer = true;
  process_ref->vm->SendMessage( CallData(nullptr, process_ref, nullptr, promise, future_time) );
  process_ref->CCallReturnVal()->promise = promise;
  return true;
}

bool GetTime( process_t process_ref )
{
  auto now = HighResClock::now().time_since_epoch();
  process_ref->CCallReturnVal()->float_val = (float_t)(std::chrono::duration_cast<std::chrono::microseconds>(now).count())/1000000.0;

  return true;
}

bool SleepMilliseconds( process_t process_ref )
{
  Object* duration = process_ref->OperandA();

  std::chrono::milliseconds duration_ms( duration->int_val );
  std::this_thread::sleep_for( duration_ms );

  return true;
}

bool IntToFloat( process_t process_ref )
{
  process_ref->CCallReturnVal()->float_val = (float_t)process_ref->OperandA()->int_val;
  return true;
}

bool FloatToInt( process_t process_ref )
{
  process_ref->CCallReturnVal()->int_val = (int_t)process_ref->OperandA()->float_val;
  return true;
}

bool IntToString( process_t process_ref )
{
  std::ostringstream string_stream;
  string_stream << process_ref->OperandA()->int_val;
  process_ref->CCallReturnVal()->string_ref = new std::string();
  *process_ref->CCallReturnVal()->string_ref = string_stream.str();

  return true;
}

bool FloatToString( process_t process_ref )
{
  std::ostringstream string_stream;
  string_stream << process_ref->OperandA()->float_val;
  process_ref->CCallReturnVal()->string_ref = new std::string();
  *process_ref->CCallReturnVal()->string_ref = string_stream.str();
  return true;
}

bool PromiseToString( process_t process_ref )
{
  std::ostringstream string_stream;
  promise_t promise = process_ref->OperandA()->promise;

  if( promise->value.object_type == (u8)ValueType::STRING )
  {
    string_stream << *promise->value.string_ref;
  }
  else if( promise->value.object_type == (u8)ValueType::INT )
  {
    string_stream << promise->value.int_val;
  }
  else if( promise->value.object_type == (u8)ValueType::FLOAT )
  {
    string_stream << promise->value.float_val;
  }

  process_ref->CCallReturnVal()->string_ref = new std::string();
  *process_ref->CCallReturnVal()->string_ref = string_stream.str();
  return true;
}

size_t CurlStoreString(void* contents, size_t size, size_t nmemb, std::string* out_string)
{
  *out_string += (char*)contents;
  return size * nmemb;
}

bool GetURL( process_t process_ref )
{
  auto dict_ref = process_ref->OperandA()->dict_ref;
  assert(dict_ref);

  auto &request = *dict_ref;

  Object* text_obj = process_ref->OperandA();
  auto text = request["url"].string_ref->c_str();

  CURL* curl = curl_easy_init();
  std::string body;
  std::string status = "OK";
  std::string usr_pwd;

  if( request.find("creds") != request.end() )
  {
    usr_pwd = *request["creds"].string_ref;
  }
  if(curl)
  {
      curl_easy_setopt(curl, CURLOPT_URL, text);

      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlStoreString);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
      curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
      if( !usr_pwd.empty() )
      {
        curl_easy_setopt(curl, CURLOPT_USERPWD, usr_pwd.c_str());
      }

      CURLcode return_code = curl_easy_perform(curl);
      // return the error code on error
      if(return_code != CURLE_OK)
      {
        status = curl_easy_strerror(return_code);
      }

      curl_easy_cleanup(curl);
  }

  process_ref->CCallReturnVal()->dict_ref = new std::unordered_map<std::string, Object>;
  auto &dict = *process_ref->CCallReturnVal()->dict_ref;
  dict["status"] = Object::BuildString(new std::string());
  *dict["status"].string_ref = status;
  dict["body"] = Object::BuildString(new std::string());
  *dict["body"].string_ref = body;

  return true;
}

bool GetURL_USERPWD( process_t process_ref )
{
  Object* text_obj = process_ref->OperandA();
  Object* usr_pwd_obj = process_ref->OperandB();
  auto text = text_obj->string_ref->c_str();
  auto usr_pwd = usr_pwd_obj->string_ref->c_str();

  CURL* curl = curl_easy_init();
  std::string response;
  if(curl)
  {
      curl_easy_setopt(curl, CURLOPT_URL, text);

      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlStoreString);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
      curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
      curl_easy_setopt(curl, CURLOPT_USERPWD, usr_pwd);

      CURLcode return_code = curl_easy_perform(curl);
      // return the error code on error
      if(return_code != CURLE_OK)
      {
        response = curl_easy_strerror(return_code);
      }

      curl_easy_cleanup(curl);
  }

  process_ref->CCallReturnVal()->string_ref = new std::string();
  *process_ref->CCallReturnVal()->string_ref = response;

  process_ref->TryCollectTempString(text_obj);

  return true;
}

} // namespace Instruction
} // namespace Eople
