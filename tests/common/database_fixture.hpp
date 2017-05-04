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

#include <eos/chain/database.hpp>
#include <eos/chain/producer_object.hpp>
#include <eos/chain/exceptions.hpp>

#include <eos/utilities/tempdir.hpp>

#include <fc/io/json.hpp>
#include <fc/smart_ref_impl.hpp>
#include <fc/signals.hpp>

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/facilities/overload.hpp>

#include <iostream>

using namespace eos::chain;

extern uint32_t EOS_TESTING_GENESIS_TIMESTAMP;

#define TEST_DB_SIZE (1024*1024*10)

#define EOS_REQUIRE_THROW( expr, exc_type )          \
{                                                         \
   std::string req_throw_info = fc::json::to_string(      \
      fc::mutable_variant_object()                        \
      ("source_file", __FILE__)                           \
      ("source_lineno", __LINE__)                         \
      ("expr", #expr)                                     \
      ("exc_type", #exc_type)                             \
      );                                                  \
   if( fc::enable_record_assert_trip )                    \
      std::cout << "EOS_REQUIRE_THROW begin "        \
         << req_throw_info << std::endl;                  \
   BOOST_REQUIRE_THROW( expr, exc_type );                 \
   if( fc::enable_record_assert_trip )                    \
      std::cout << "EOS_REQUIRE_THROW end "          \
         << req_throw_info << std::endl;                  \
}

#define EOS_CHECK_THROW( expr, exc_type )            \
{                                                         \
   std::string req_throw_info = fc::json::to_string(      \
      fc::mutable_variant_object()                        \
      ("source_file", __FILE__)                           \
      ("source_lineno", __LINE__)                         \
      ("expr", #expr)                                     \
      ("exc_type", #exc_type)                             \
      );                                                  \
   if( fc::enable_record_assert_trip )                    \
      std::cout << "EOS_CHECK_THROW begin "          \
         << req_throw_info << std::endl;                  \
   BOOST_CHECK_THROW( expr, exc_type );                   \
   if( fc::enable_record_assert_trip )                    \
      std::cout << "EOS_CHECK_THROW end "            \
         << req_throw_info << std::endl;                  \
}

namespace eos { namespace chain {
FC_DECLARE_EXCEPTION(testing_exception, 6000000, "test framework exception")
FC_DECLARE_DERIVED_EXCEPTION(missing_key_exception, eos::chain::testing_exception, 6010000, "key could not be found")

/**
 * @brief The testing_fixture class provides various services relevant to testing the database.
 */
class testing_fixture {
public:
   testing_fixture();

   /**
    * @brief Get a temporary directory to store data in during this test
    *
    * @param id Identifier for the temporary directory. All requests for directories with the same ID will receive the
    * same path. If id is empty, a unique directory will be returned.
    *
    * @return Path to the temporary directory
    *
    * This method makes it easy to get temporary directories for the duration of a test. All returned directories will
    * be automatically deleted when the testing_fixture is destroyed.
    *
    * If multiple calls to this function are made with the same id, the same path will be returned. Multiple calls with
    * distinct ids will not return the same path. If called with an empty id, a unique path will be returned which will
    * not be returned from any subsequent call.
    */
   fc::path get_temp_dir(std::string id = std::string());

   const genesis_state_type& genesis_state() const;
   genesis_state_type& genesis_state();

   private_key_type get_private_key(const public_key_type& public_key) const;

protected:
   std::vector<fc::temp_directory> anonymous_temp_dirs;
   std::map<std::string, fc::temp_directory> named_temp_dirs;
   std::map<public_key_type, private_key_type> key_ring;
   genesis_state_type default_genesis_state;
};

/**
 * @brief The testing_database class wraps database and eliminates some of the boilerplate for common operations on the
 * database during testing.
 *
 * testing_databases have an optional ID, which is passed to the constructor. If two testing_databases are created with
 * the same ID, they will have the same data directory. If no ID, or an empty ID, is provided, the database will have a
 * unique data directory which no subsequent testing_database will be assigned.
 *
 * testing_database helps with producing blocks, or missing blocks, via the @ref produce_blocks and @ref miss_blocks
 * methods. To produce N blocks, simply call produce_blocks(N); to miss N blocks, call miss_blocks(N). Note that missing
 * blocks has no effect on the database until the next block, following the missed blocks, is produced.
 */
class testing_database : public database {
public:
   testing_database(testing_fixture& fixture, std::string id = std::string(),
                    fc::optional<genesis_state_type> override_genesis_state = {});

   /**
    * @brief Open the database using the boilerplate testing database settings
    */
   void open();
   /**
    * @brief Reindex the database using the boilerplate testing database settings
    */
   void replay();
   /**
    * @brief Wipe the database using the boilerplate testing database settings
    * @param include_blocks If true, the blocks will be removed as well; otherwise, only the database will be wiped and
    * can then be rebuilt from the local blocks
    */
   void wipe(bool include_blocks = true);

   /**
    * @brief Produce new blocks, adding them to the database, optionally following a gap of missed blocks
    * @param count Number of blocks to produce
    * @param blocks_to_miss Number of block intervals to miss a production before producing the next block
    *
    * Creates and adds  @ref count new blocks to the database, after going @ref blocks_to_miss intervals without
    * producing a block.
    */
   void produce_blocks(uint32_t count = 1, uint32_t blocks_to_miss = 0);

   /**
    * @brief Sync this database with other
    * @param other Database to sync with
    *
    * To sync the databases, all blocks from one database which are unknown to the other are pushed to the other, then
    * the same thing in reverse. Whichever database has more blocks will have its blocks sent to the other first.
    *
    * Blocks not on the main fork are ignored.
    */
   void sync_with(testing_database& other);

protected:
   fc::path data_dir;
   genesis_state_type genesis_state;

   testing_fixture& fixture;
};

/// Some helpful macros to reduce boilerplate when making testing_databases @{
#define MKDB1(name) testing_database name(*this); name.open();
#define MKDB2(name, id) testing_database name(*this, #id); name.open();
/**
 * @brief Create/Open a testing_database, optionally with an ID
 *
 * Creates and opens a testing_database with the first argument as its name, and, if present, the second argument as
 * its ID. The ID should be provided without quotes.
 *
 * Example:
 * @code{.cpp}
 * // Create testing_databases db1 and db2, with db2 having ID "id2"
 * MKDB(db1)
 * MKDB(db2, id2)
 * @endcode
 */
#define MKDB(...) BOOST_PP_OVERLOAD(MKDB, __VA_ARGS__)(__VA_ARGS__)
#define MKDBS_MACRO(x, y, name) MKDB(name)
/**
 * @brief Similar to @ref MKDB, but works with several databases at once
 *
 * Creates and opens several testing_databases
 *
 * Example:
 * @code{.cpp}
 * // Create testing_databases db1 and db2, with db2 having ID "id2"
 * MKDBS((db1)(db2, id2))
 * @endcode
 */
#define MKDBS(...) BOOST_PP_SEQ_FOR_EACH(MKDBS_MACRO, _, __VA_ARGS__)
/// @}

/**
 * @brief The testing_network class provides a simplistic virtual P2P network connecting testing_databases together.
 *
 * A testing_database may be connected to zero or more testing_networks at any given time. When a new testing_database
 * joins the network, it will be synced with all other databases already in the network (blocks known only to the new
 * database will be pushed to the prior network members and vice versa, ignoring blocks not on the main fork). After
 * this, whenever any database in the network gets a new block, that block will be pushed to all other databases in the
 * network as well.
 */
class testing_network {
public:
   /**
    * @brief Add a new database to the network
    * @param new_database The database to add
    */
   void connect_database(testing_database& new_database);
   /**
    * @brief Remove a database from the network
    * @param leaving_database The database to remove
    */
   void disconnect_database(testing_database& leaving_database);
   /**
    * @brief Disconnect all databases from the network
    */
   void disconnect_all();

   /**
    * @brief Send a block to all databases in this network
    * @param block The block to send
    */
   void propagate_block(const signed_block& block, const testing_database& skip_db);

protected:
   std::map<testing_database*, fc::scoped_connection> databases;
};

/// Some helpful macros to reduce boilerplate when making a testing_network and connecting some testing_databases @{
#define MKNET1(name) testing_network name;
#define MKNET2_MACRO(x, name, db) name.connect_database(db);
#define MKNET2(name, ...) MKNET1(name) BOOST_PP_SEQ_FOR_EACH(MKNET2_MACRO, name, __VA_ARGS__)
/**
 * @brief MKNET is a shorthand way to create a testing_network and connect some testing_databases to it.
 *
 * Example usage:
 * @code{.cpp}
 * // Create and open testing_databases named alice, bob, and charlie
 * MKDBS((alice)(bob)(charlie))
 * // Create a testing_network named net and connect alice and bob to it
 * MKNET(net, (alice)(bob))
 *
 * // Connect charlie to net, then disconnect alice
 * net.connect_database(charlie);
 * net.disconnect_database(alice);
 *
 * // Create a testing_network named net2 with no databases connected
 * MKNET(net2)
 * @endcode
 */
#define MKNET(...) BOOST_PP_OVERLOAD(MKNET, __VA_ARGS__)(__VA_ARGS__)
/// @}

/**
 * @brief MKKEY is a shorthand way to create a keypair
 *
 * @code{.cpp}
 * // This line:
 * MKKEY(a_key)
 * // ...defines these objects:
 * private_key_type a_key_private_key;
 * PublicKey a_key_public_key;
 * // The private key is generated off of the sha256 hash of "a_key_private_key", so it should be unique from all
 * // other keys created with MKKEY in the same scope.
 * @endcode
 */
#define MKKEY(name) auto name ## _private_key = private_key_type::regenerate(fc::digest(#name "_private_key")); \
   PublicKey name ## _public_key = name ## _private_key.get_public_key();

/**
 * @brief AUTHK is a shorthand way to create an inline Authority based on a key
 *
 * Invoke AUTHK passing the name of a public key in the current scope, and AUTHK will resolve inline to an authority
 * which can be satisfied by a signature generated by the corresponding private key.
 */
#define AUTHK(pubkey) (Authority{1, {{pubkey, 1}}, {}})
/**
 * @brief AUTHA is a shorthand way to create an inline Authority based on an account
 *
 * Invoke AUTHA passing the name of an account, and AUTHA will resolve inline to an authority which can be satisfied by
 * the provided account's active authority.
 */
#define AUTHA(account) (Authority{1, {}, {{{#account, "active"}, 1}}})

#define MKACCT_IMPL(db, name, creator, active, owner, recovery, deposit) \
   { \
      chain::SignedTransaction trx; \
      trx.emplaceMessage(#creator, "sys", vector<AccountName>{}, "CreateAccount", \
                         types::CreateAccount{#creator, #name, owner, active, recovery, deposit}); \
      trx.expiration = db.head_block_time() + 100; \
      trx.set_reference_block(db.head_block_id()); \
      db.push_transaction(trx); \
   }
#define MKACCT2(db, name) \
   MKKEY(name) \
   MKACCT_IMPL(db, name, init0, AUTHK(name ## _public_key), AUTHK(name ## _public_key), AUTHA(init0), Asset(100))
#define MKACCT3(db, name, creator) \
   MKKEY(name) \
   MKACCT_IMPL(db, name, creator, AUTHK(name ## _public_key), AUTHK(name ## _public_key), AUTHA(creator), Asset(100))
#define MKACCT4(db, name, creator, deposit) \
   MKKEY(name) \
   MKACCT_IMPL(db, name, creator, AUTHK(name ## _public_key), AUTHK(name ## _public_key), AUTHA(creator), deposit)
#define MKACCT5(db, name, creator, deposit, owner) \
   MKKEY(name) \
   MKACCT_IMPL(db, name, creator, owner, AUTHK(name ## _public_key), AUTHA(creator), deposit)
#define MKACCT6(db, name, creator, deposit, owner, active) \
   MKACCT_IMPL(db, name, creator, owner, active, AUTHA(creator), deposit)
#define MKACCT7(db, name, creator, deposit, owner, active, recovery) \
   MKACCT_IMPL(db, name, creator, owner, active, recovery, deposit)
/**
 * @brief MKACCT is a shorthand way to create an account
 *
 * Use MKACCT to create an account, including keys. The changes will be applied via a transaction applied to the
 * provided database object. The changes will not be incorporated into a block; they will be left in the pending state.
 *
 * Example:
 * @code{.cpp}
 * MKACCT(db, joe)
 * // ... creates these objects:
 * private_key_type joe_private_key;
 * PublicKey joe_public_key;
 * // ...and also registers the account joe with owner and active authorities satisfied by these keys, created by
 * // init0, with init0's active authority as joe's recovery authority, and initially endowed with Asset(100)
 * @endcode
 *
 * You may specify a third argument for the creating account:
 * @code{.cpp}
 * // Same as MKACCT(db, joe) except that sam will create joe's account instead of init0
 * MKACCT(db, joe, sam)
 * @endcode
 *
 * You may specify a fourth argument for the amount to transfer in account creation:
 * @code{.cpp}
 * // Same as MKACCT(db, joe, sam) except that sam will send joe ASSET(500) during creation
 * MKACCT(db, joe, sam, Asset(100))
 * @endcode
 *
 * You may specify a fifth argument, which will be used as the owner authority (must be an Authority, NOT a key!).
 *
 * You may specify a sixth argument, which will be used as the active authority. If six or more arguments are provided,
 * the default keypair will NOT be created or put into scope.
 *
 * You may specify a seventh argument, which will be used as the recovery authority.
 */
#define MKACCT(...) BOOST_PP_OVERLOAD(MKACCT, __VA_ARGS__)(__VA_ARGS__)

#define XFER5(db, sender, recipient, amount, memo) \
   { \
      chain::SignedTransaction trx; \
      trx.emplaceMessage(#sender, "sys", vector<AccountName>{#recipient}, "Transfer", \
                                types::Transfer{#sender, #recipient, amount, memo}); \
      trx.expiration = db.head_block_time() + 100; \
      trx.set_reference_block(db.head_block_id()); \
      db.push_transaction(trx); \
   }
#define XFER4(db, sender, recipient, amount) XFER5(db, sender, recipient, amount, "")
/**
 * @brief Shorthand way to transfer funds
 *
 * Use XFER to send funds from one account to another:
 * @code{.cpp}
 * // Send 10 EOS from alice to bob
 * XFER(db, alice, bob, Asset(10));
 *
 * // Send 10 EOS from alice to bob with memo "Thanks for all the fish!"
 * XFER(db, alice, bob, Asset(10), "Thanks for all the fish!");
 * @endcode
 *
 * The changes will be applied via a transaction applied to the provided database object. The changes will not be
 * incorporated into a block; they will be left in the pending state.
 */
#define XFER(...) BOOST_PP_OVERLOAD(XFER, __VA_ARGS__)(__VA_ARGS__)

#define MKPDCR3(db, owner, key) \
   { \
      chain::SignedTransaction trx; \
      trx.emplaceMessage(#owner, "sys", vector<AccountName>{}, "CreateProducer", \
                                types::CreateProducer{#owner, key}); \
      trx.expiration = db.head_block_time() + 100; \
      trx.set_reference_block(db.head_block_id()); \
      db.push_transaction(trx); \
   }
#define MKPDCR2(db, owner) \
   MKKEY(owner ## _producer); \
   MKPDCR3(db, owner, owner ## _producer_public_key);
/**
 * @brief Shorthand way to create a block producer
 *
 * Use MKPDCR to create a block producer:
 * @code{.cpp}
 * // Create a block producer belonging to joe using signing_key as the block signing key:
 * MKPDCR(db, joe, signing_key);
 *
 * // Create a block producer belonging to joe, using a new key as the block signing key:
 * MKPDCR(db, joe);
 * // ... creates the objects:
 * private_key_type joe_producer_private_key;
 * PublicKey joe_producer_public_key;
 * @endcode
 */
#define MKPDCR(...) BOOST_PP_OVERLOAD(MKPDCR, __VA_ARGS__)(__VA_ARGS__)

/**
 * @brief Shorthand way to update a block producer
 *
 * Use UPPDCR to update a block producer:
 * @code{.cpp}
 * // Update a block producer belonging to joe using signing_key as the new block signing key:
 * UPPDCR(db, joe, signing_key)
 * @endcode
 */
#define UPPDCR(db, owner, key) \
   { \
      chain::SignedTransaction trx; \
      trx.emplaceMessage(#owner, "sys", vector<AccountName>{}, "UpdateProducer", \
                                types::UpdateProducer{#owner, key}); \
      trx.expiration = db.head_block_time() + 100; \
      trx.set_reference_block(db.head_block_id()); \
      db.push_transaction(trx); \
   }
} }
