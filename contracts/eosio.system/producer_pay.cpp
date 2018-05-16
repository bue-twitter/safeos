#include "eosio.system.hpp"

#include <eosio.token/eosio.token.hpp>

namespace eosiosystem {

   const int64_t  min_daily_tokens    = 100;
   const int64_t  min_activated_stake = 150'000'000'0000;
   const double   continuous_rate     = 0.04879;          // 5% annual rate
   const double   perblock_rate       = 0.0025;           // 0.25%
   const double   standby_rate        = 0.0075;           // 0.75%
   const uint32_t blocks_per_year     = 52*7*24*2*3600;   // half seconds per year
   const uint32_t seconds_per_year    = 52*7*24*3600;
   const uint32_t blocks_per_day      = 2 * 24 * 3600;
   const uint32_t blocks_per_hour     = 2 * 3600;
   const uint64_t useconds_per_day    = 24 * 3600 * uint64_t(1000000);
   const uint64_t useconds_per_year   = seconds_per_year*1000000ll;


   void system_contract::onblock( block_timestamp timestamp, account_name producer ) {
      using namespace eosio;

      require_auth(N(eosio));

      /** until activated stake crosses this threshold no new rewards are paid */
      if( _gstate.total_activated_stake < min_activated_stake )
         return;

      if( _gstate.last_pervote_bucket_fill == 0 )  /// start the presses
         _gstate.last_pervote_bucket_fill = current_time();


      /**
       * At startup the initial producer may not be one that is registered / elected
       * and therefore there may be no producer object for them.
       */
      auto prod = _producers.find(producer);
      if ( prod != _producers.end() ) {
         _gstate.total_unpaid_blocks++;
         _producers.modify( prod, 0, [&](auto& p ) {
               p.unpaid_blocks++;
               p.last_produced_block_time = timestamp;
         });
      }
      
      /// only update block producers once every minute, block_timestamp is in half seconds
      if( timestamp.slot - _gstate.last_producer_schedule_update.slot > 120 ) {
         update_elected_producers( timestamp );
      }

   }
   
   eosio::asset system_contract::payment_per_vote( const account_name& owner, double owners_votes, const eosio::asset& pervote_bucket ) {
      eosio::asset payment(0, S(4,SYS));
      const int64_t min_daily_amount = 100 * 10000;
      if ( pervote_bucket.amount < min_daily_amount ) {
         return payment;
      }
      
      auto idx = _producers.template get_index<N(prototalvote)>();
      
      double total_producer_votes   = 0;   
      double running_payment_amount = 0;
      bool   to_be_payed            = false;
      for ( auto itr = idx.cbegin(); itr != idx.cend(); ++itr ) {
         if ( !(itr->total_votes > 0) ) {
            break;
         }
         if ( !itr->active() ) {
            continue;
         }
         
         if ( itr->owner == owner ) {
            to_be_payed = true;
         }
         
         total_producer_votes += itr->total_votes;
         running_payment_amount = (itr->total_votes) * double(pervote_bucket.amount) / total_producer_votes;
         if ( running_payment_amount < min_daily_amount ) {
            if ( itr->owner == owner ) {
               to_be_payed = false;
            }
            total_producer_votes -= itr->total_votes;
            break;
         }
      }
      
      if ( to_be_payed ) {
         payment.amount = static_cast<int64_t>( (double(pervote_bucket.amount) * owners_votes) / total_producer_votes );
      }
      
      return payment;

   }

   using namespace eosio;
   void system_contract::claimrewards( const account_name& owner ) {
      require_auth(owner);

      const auto& prod = _producers.get( owner );
      eosio_assert( prod.active(), "producer does not have an active key" );
      
      eosio_assert( _gstate.total_activated_stake >= min_activated_stake, "not enough has been staked for producers to claim rewards" );

      auto ct = current_time();

      eosio_assert( ct - prod.last_claim_time > useconds_per_day, "already claimed rewards within past day" );

      const asset token_supply   = token( N(eosio.token)).get_supply(symbol_type(system_token_symbol).name() );
      const auto usecs_since_last_fill = ct - _gstate.last_pervote_bucket_fill;

      if( usecs_since_last_fill > 0 ) {
         auto new_tokens = static_cast<int64_t>( (continuous_rate * double(token_supply.amount) * double(usecs_since_last_fill)) / double(useconds_per_year) );

         auto to_producers       = new_tokens / 5;
         auto to_savings         = new_tokens - to_producers;
         auto to_per_block_pay   = to_producers / 4;
         auto to_per_vote_pay    = to_producers - to_per_block_pay;

         INLINE_ACTION_SENDER(eosio::token, issue)( N(eosio.token), {{N(eosio),N(active)}},
                                                    {N(eosio), asset(new_tokens), std::string("issue tokens for producer pay and savings")} );

         _gstate.pervote_bucket  += to_per_vote_pay;
         _gstate.perblock_bucket += to_per_block_pay;
         _gstate.savings         += to_savings;

         _gstate.last_pervote_bucket_fill = ct;
      }

      int64_t producer_per_block_pay = 0;
      if( _gstate.total_unpaid_blocks > 0 ) {
         producer_per_block_pay = (_gstate.perblock_bucket * prod.unpaid_blocks) / _gstate.total_unpaid_blocks;
      }
      int64_t producer_per_vote_pay = 0;
      if( _gstate.total_producer_vote_weight > 0 ) { 
         producer_per_vote_pay  = int64_t((_gstate.pervote_bucket * prod.total_votes ) / _gstate.total_producer_vote_weight);
      }
      if( producer_per_vote_pay < 100'0000 ) {
         producer_per_vote_pay = 0;
      }
      int64_t total_pay            = producer_per_block_pay + producer_per_vote_pay;

      _gstate.pervote_bucket      -= producer_per_vote_pay;
      _gstate.perblock_bucket     -= producer_per_block_pay;
      _gstate.total_unpaid_blocks -= prod.unpaid_blocks;

      _producers.modify( prod, 0, [&](auto& p) {
          p.last_claim_time = ct;
          p.unpaid_blocks = 0;
      });
      
      if( total_pay > 0 ) {
         INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(eosio),N(active)},
                                                       { N(eosio), owner, asset(total_pay), std::string("producer pay") } );
      }
   }

} //namespace eosiosystem
