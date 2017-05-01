/*
 * Copyright (c) 2017, Respective Authors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once
#include <eos/chain/types.hpp>
#include <eos/chain/authority.hpp>

#include "multi_index_includes.hpp"

namespace eos { namespace chain {

   struct shared_authority {
      shared_authority( chainbase::allocator<char> alloc )
      :accounts(alloc),keys(alloc)
      {}

      shared_authority& operator=(const Authority& a) {
         threshold = a.threshold;
         accounts = decltype(accounts)(a.accounts.begin(), a.accounts.end(), accounts.get_allocator());
         keys = decltype(keys)(a.keys.begin(), a.keys.end(), keys.get_allocator());
         return *this;
      }
      shared_authority& operator=(Authority&& a) {
         threshold = a.threshold;
         accounts.reserve(a.accounts.size());
         for (auto& p : a.accounts)
            accounts.emplace_back(std::move(p));
         keys.reserve(a.keys.size());
         for (auto& p : a.keys)
            keys.emplace_back(std::move(p));
         return *this;
      }

      UInt32                                 threshold = 0;
      shared_vector<AccountPermissionWeight> accounts;
      shared_vector<KeyPermissionWeight>     keys;
   };
   
   class account_object : public chainbase::object<account_object_type, account_object>
   {
      OBJECT_CTOR(account_object)

      id_type           id;
      AccountName       name;
      Asset             balance;
      UInt64            votes                  = 0;
      UInt64            converting_votes       = 0;
      Time              last_vote_conversion;
   };

   struct by_name;
   using account_index = chainbase::shared_multi_index_container<
      account_object,
      indexed_by<
         ordered_unique<tag<by_id>, member<account_object, account_object::id_type, &account_object::id>>,
         ordered_unique<tag<by_name>, member<account_object, AccountName, &account_object::name>>
      >
   >;

   class permission_object : public chainbase::object<permission_object_type, permission_object>
   {
      OBJECT_CTOR(permission_object, (auth) )

      id_type           id;
      account_id_type   owner; ///< the account this permission belongs to
      id_type           parent; ///< parent permission 
      permission_name   name;
      shared_authority  auth; ///< TODO
   };



   struct by_parent;
   struct by_owner;
   using permission_index = chainbase::shared_multi_index_container<
      permission_object,
      indexed_by<
         ordered_unique<tag<by_id>, member<permission_object, permission_object::id_type, &permission_object::id>>,
         ordered_unique<tag<by_parent>, 
            composite_key< permission_object,
               member<permission_object, permission_object::id_type, &permission_object::parent>,
               member<permission_object, permission_object::id_type, &permission_object::id>
            >
         >,
         ordered_unique<tag<by_owner>, 
            composite_key< permission_object,
               member<permission_object, account_object::id_type, &permission_object::owner>,
               member<permission_object, permission_name, &permission_object::name>,
               member<permission_object, permission_object::id_type, &permission_object::id>
            >
         >,
         ordered_unique<tag<by_name>, member<permission_object, permission_name, &permission_object::name> >
      >
   >;


   /**
    * This table defines all of the event handlers for every contract.  Every message is
    * delivered TO a particular contract and also processed in parallel by several other contracts. 
    *
    * Each account can define a custom handler based upon the tuple { processor, recipient, type } where
    * processor is the account that is processing the message, recipient is the account specified by
    * message::recipient and type is messagse::type.
    * 
    *
    */
   class action_code_object : public chainbase::object<action_code_object_type, action_code_object>
   {
      OBJECT_CTOR(action_code_object, (validate_action)(validate_precondition)(apply) )

      id_type                        id;
      account_id_type                recipient;
      account_id_type                processor; 
      TypeName                       type; ///< the name of the action (defines serialization)

      shared_string   validate_action; ///< read only access to action
      shared_string   validate_precondition; ///< read only access to state
      shared_string   apply; ///< the code that executes the state transition
   };

   struct by_parent;
   struct by_processor_recipient_type;
   using action_code_index = chainbase::shared_multi_index_container<
      action_code_object,
      indexed_by<
         ordered_unique<tag<by_id>, member<action_code_object, action_code_object::id_type, &action_code_object::id>>,
         ordered_unique<tag<by_processor_recipient_type>, 
            composite_key< action_code_object,
               member<action_code_object, account_id_type, &action_code_object::processor>,
               member<action_code_object, account_id_type, &action_code_object::recipient>,
               member<action_code_object, TypeName, &action_code_object::type>
            >
         >
      >
   >;


   /**
    *  Maps the permission level on the code to the permission level specififed by owner, when specifying a contract the
    *  contract will specify 1 permission_object per action, and by default the parent of that permission object will be
    *  the active permission of the contract; however, the contract owner could group their actions any way they like.
    *
    *  When it comes time to evaluate whether User can call Action on Contract with UserPermissionLevel the algorithm 
    *  operates as follows:
    *
    *  let scope_permission = action_code.permission
    *  while( ! mapping for (scope_permission / owner )
    *    scope_permission = scope_permission.parent
    *    if( !scope_permission ) 
    *       user permission => active 
    *       break;
    *
    *   Now that we know the required user permission...
    *
    *   while( ! transaction.has( user_permission ) )
    *       user_permission = user_permission.parent
    *       if( !user_permission )
    *          throw invalid permission
    *   pass
    */
   class action_permission_object : public chainbase::object<action_permission_object_type, action_permission_object>
   {
      OBJECT_CTOR(action_permission_object)

      id_type                        id;
      account_id_type                owner; ///< the account whose permission we seek
      permission_object::id_type     scope_permission; ///< the scope permission defined by the contract for the action
      permission_object::id_type     owner_permission; ///< the owner permission that is required
   };

   struct by_owner_scope;
   using action_permission_index = chainbase::shared_multi_index_container<
      action_permission_object,
      indexed_by<
         ordered_unique<tag<by_id>, member<action_permission_object, action_permission_object::id_type, &action_permission_object::id>>,
         ordered_unique<tag<by_owner_scope>, 
            composite_key< action_permission_object,
               member<action_permission_object, account_id_type, &action_permission_object::owner>,
               member<action_permission_object, permission_object::id_type, &action_permission_object::scope_permission>
            >
         >
      >
   >;

   struct type_object : public chainbase::object<type_object_type, type_object> {
      OBJECT_CTOR(type_object, (fields))

      id_type               id;
      account_name          scope;
      TypeName              name;
      account_name          base_scope;
      TypeName              base;
      shared_vector<Field>  fields;
   };

   struct by_scope_name;
   using type_index = chainbase::shared_multi_index_container<
      type_object,
      indexed_by<
         ordered_unique<tag<by_id>, member<type_object, type_object::id_type, &type_object::id>>,
         ordered_unique<tag<by_scope_name>, 
            composite_key< type_object,
               member<type_object, account_name, &type_object::scope>,
               member<type_object, message_type, &type_object::name>
            >
         >
      >
   >;

   struct key_value_object : public chainbase::object<key_value_object_type, key_value_object> {
      OBJECT_CTOR(key_value_object, (key)(value))

      id_type               id;
      account_name          scope;
      shared_string         key;
      shared_string         value;
   };

   struct by_scope_key;
   using key_value_index = chainbase::shared_multi_index_container<
      key_value_object,
      indexed_by<
         ordered_unique<tag<by_id>, member<key_value_object, key_value_object::id_type, &key_value_object::id>>,
         ordered_unique<tag<by_scope_key>, 
            composite_key< key_value_object,
               member<key_value_object, account_name, &key_value_object::scope>,
               member<key_value_object, shared_string, &key_value_object::key>
            >,
            composite_key_compare< std::less<account_name>, chainbase::strcmp_less >
         >
      >
   >;


} } // eos::chain

CHAINBASE_SET_INDEX_TYPE(eos::chain::account_object, eos::chain::account_index)
CHAINBASE_SET_INDEX_TYPE(eos::chain::permission_object, eos::chain::permission_index)
CHAINBASE_SET_INDEX_TYPE(eos::chain::action_code_object, eos::chain::action_code_index)
CHAINBASE_SET_INDEX_TYPE(eos::chain::action_permission_object, eos::chain::action_permission_index)
CHAINBASE_SET_INDEX_TYPE(eos::chain::type_object, eos::chain::type_index)
CHAINBASE_SET_INDEX_TYPE(eos::chain::key_value_object, eos::chain::key_value_index)

FC_REFLECT(eos::chain::account_object, (id)(name)(balance)(votes)(converting_votes)(last_vote_conversion) )
FC_REFLECT(eos::chain::permission_object, (id)(owner)(parent)(name) )
FC_REFLECT(eos::chain::action_code_object, (id)(recipient)(processor)(type)(validate_action)(validate_precondition)(apply) )
FC_REFLECT(eos::chain::action_permission_object, (id)(owner)(owner_permission)(scope_permission) )
FC_REFLECT(eos::chain::type_object, (id)(scope)(name)(base_scope)(base)(fields) )
FC_REFLECT(eos::chain::key_value_object, (id)(scope)(key)(value) )
