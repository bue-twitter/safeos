#include <eos/chain_plugin/chain_plugin.hpp>
#include <eos/chain/fork_database.hpp>
#include <eos/chain/block_log.hpp>
#include <eos/chain/exceptions.hpp>
#include <fc/io/json.hpp>

namespace eos {

using namespace eos;
using fc::flat_map;
using chain::block_id_type;
using chain::fork_database;
using chain::block_log;

class chain_plugin_impl {
public:
   bfs::path                        block_log_file;
   bfs::path                        genesis_file;
   bool                             readonly = false;
   flat_map<uint32_t,block_id_type> loaded_checkpoints;

   fc::optional<fork_database>      fork_db;
   fc::optional<block_log>          block_logger;
   fc::optional<chain_controller>   chain;
};


chain_plugin::chain_plugin()
:my(new chain_plugin_impl()) {
}

chain_plugin::~chain_plugin(){}

void chain_plugin::set_program_options(options_description& cli, options_description& cfg)
{
   cfg.add_options()
         ("genesis-json", bpo::value<boost::filesystem::path>(), "File to read Genesis State from")
         ("block-log-dir", bpo::value<bfs::path>()->default_value("blocks.log"),
          "the location of the block log (absolute path or relative to application data dir)")
         ("checkpoint,c", bpo::value<vector<string>>()->composing(), "Pairs of [BLOCK_NUM,BLOCK_ID] that should be enforced as checkpoints.")
         ;
   cli.add_options()
         ("replay-blockchain", bpo::bool_switch()->default_value(false),
          "clear chain database and replay all blocks")
         ("resync-blockchain", bpo::bool_switch()->default_value(false),
          "clear chain database and block log")
         ;
}

void chain_plugin::plugin_initialize(const variables_map& options) {
   ilog("initializing chain plugin");

   if(options.count("genesis-json")) {
      my->genesis_file = options.at("genesis-json").as<bfs::path>();
   }
   if (options.count("block-log-dir")) {
      auto bld = options.at("block-log-dir").as<bfs::path>();
      if(bld.is_relative())
         my->block_log_file = app().data_dir() / bld;
      else
         my->block_log_file = bld;
   }

   if (options.at("replay-blockchain").as<bool>()) {
      ilog("Replay requested: wiping database");
      app().get_plugin<database_plugin>().wipe_database();
   }
   if (options.at("resync-blockchain").as<bool>()) {
      ilog("Resync requested: wiping blocks");
      app().get_plugin<database_plugin>().wipe_database();
      fc::remove_all(my->block_log_file);
   }

   if(options.count("checkpoint"))
   {
      auto cps = options.at("checkpoint").as<vector<string>>();
      my->loaded_checkpoints.reserve(cps.size());
      for(auto cp : cps)
      {
         auto item = fc::json::from_string(cp).as<std::pair<uint32_t,block_id_type>>();
         my->loaded_checkpoints[item.first] = item.second;
      }
   }
}

void chain_plugin::plugin_startup() {
   auto genesis_loader = [this] {
      if (my->genesis_file.empty())
         return eos::chain::genesis_state_type();
      return fc::json::from_file(my->genesis_file).as<eos::chain::genesis_state_type>();
   };
   auto& db = app().get_plugin<database_plugin>().db();

   my->fork_db = fork_database();
   my->block_logger = block_log(my->block_log_file);
   my->chain = chain_controller(db, *my->fork_db, *my->block_logger, genesis_loader);

   if(!my->readonly) {
      ilog("starting chain in read/write mode");
      my->chain->add_checkpoints(my->loaded_checkpoints);
   }

   ilog("Blockchain started; head block is #${num}", ("num", my->chain->head_block_num()));
}

void chain_plugin::plugin_shutdown() {
}

bool chain_plugin::accept_block(const chain::signed_block& block, bool currently_syncing) {
   if (currently_syncing && block.block_num() % 10000 == 0) {
      ilog("Syncing Blockchain --- Got block: #${n} time: ${t} producer: ${p}",
           ("t", block.timestamp)
           ("n", block.block_num())
           ("p", block.producer));
   }

   return chain().push_block(block);
}

void chain_plugin::accept_transaction(const chain::SignedTransaction& trx) {
   chain().push_transaction(trx);
}

bool chain_plugin::block_is_on_preferred_chain(const chain::block_id_type& block_id) {
   // If it's not known, it's not preferred.
   if (!chain().is_known_block(block_id)) return false;
   // Extract the block number from block_id, and fetch that block number's ID from the database.
   // If the database's block ID matches block_id, then block_id is on the preferred chain. Otherwise, it's on a fork.
   return chain().get_block_id_for_num(chain::block_header::num_from_id(block_id)) == block_id;
}

chain_controller& chain_plugin::chain() { return *my->chain; }
const chain::chain_controller& chain_plugin::chain() const { return *my->chain; }

namespace chain_apis {

read_only::get_info_results read_only::get_info(const read_only::get_info_params&) const {
   return {
      db.head_block_num(),
      db.head_block_id(),
      db.head_block_time(),
      db.head_block_producer(),
      std::bitset<64>(db.get_dynamic_global_properties().recent_slots_filled).to_string(),
      __builtin_popcountll(db.get_dynamic_global_properties().recent_slots_filled) / 64.0
   };
}

read_only::get_block_results read_only::get_block(const read_only::get_block_params& params) const {
   try {
      if (auto block = db.fetch_block_by_id(fc::json::from_string(params.block_num_or_id).as<chain::block_id_type>()))
         return *block;
   } catch (fc::bad_cast_exception) {/* do nothing */}
   try {
      if (auto block = db.fetch_block_by_number(fc::to_uint64(params.block_num_or_id)))
         return *block;
   } catch (fc::bad_cast_exception) {/* do nothing */}

   FC_THROW_EXCEPTION(chain::unknown_block_exception,
                      "Could not find block: ${block}", ("block", params.block_num_or_id));
}

read_write::push_block_results read_write::push_block(const read_write::push_block_params& params) {
   db.push_block(params);
   return read_write::push_block_results();
}

read_write::push_transaction_results read_write::push_transaction(const read_write::push_transaction_params& params) {
   db.push_transaction(params);
   return read_write::push_transaction_results();
}

} // namespace chain_apis
} // namespace eos
