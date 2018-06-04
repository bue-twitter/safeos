#include <eosiolib/types.h>
#include <eosiolib/privileged.hpp>

#if 0

#include <eosiolib/datastream.hpp>


namespace eosio {

   void set_blockchain_parameters(const eosio::blockchain_parameters& params) {
      char buf[sizeof(eosio::blockchain_parameters)];
      eosio::datastream<char *> ds( buf, sizeof(buf) );
      ds << params;
      set_blockchain_parameters_packed( buf, ds.tellp() );
   }

   void get_blockchain_parameters(eosio::blockchain_parameters& params) {
      char buf[sizeof(eosio::blockchain_parameters)];
      size_t size = get_blockchain_parameters_packed( buf, sizeof(buf) );
      eosio_assert( size <= sizeof(buf), "buffer is too small" );
      eosio::datastream<const char*> ds( buf, size_t(size) );
      ds >> params;
   }

} /// namespace eosio

#endif

namespace eosio {

   void set_blockchain_parameters(const eosio::blockchain_parameters& params) {
   }

   void get_blockchain_parameters(eosio::blockchain_parameters& params) {
   }

} /// namespace eosio

#include "wasm_api.h"
static struct wasm_api s_api;
extern "C" void register_wasm_api(struct wasm_api* api) {
   s_api = *api;
}

extern "C" struct wasm_api* get_wasm_api() {
   return &s_api;
}

