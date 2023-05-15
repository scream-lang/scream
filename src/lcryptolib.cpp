#define lhashlib_c
#define MASK_LIB

#include "mask.h"
#include "masklib.h"
#include "lprefix.h"
#include "maskconf.h"
#include "lauxlib.h"
#include "lstring.h"
#include "lcryptolib.hpp"

#include <ios>
#include <string>
#include <random>
#include <sstream>
#include <iomanip>


static int fnv1(mask_State *L)
{
  const auto FNV_offset_basis = 0xcbf29ce484222325;
  const auto FNV_prime = 0x100000001b3;
  const std::string s  = maskL_checkstring(L, 1);
  mask_Integer hash     = FNV_offset_basis;
  
  for (auto& c : s)
  {
    hash *= FNV_prime;
    hash ^= c;
  }

  mask_pushinteger(L, hash);
  return 1;
}


static int fnv1a(mask_State *L)
{
  const auto FNV_offset_basis = 0xcbf29ce484222325;
  const auto FNV_prime = 0x100000001b3;
  const std::string s  = maskL_checkstring(L, 1);
  mask_Integer hash     = FNV_offset_basis;
  
  for (auto& c : s)
  {
    hash ^= c;
    hash *= FNV_prime;
  }

  mask_pushinteger(L, hash);
  return 1;
}


static int joaat(mask_State *L)
{
  /* get input */
  size_t size;
  const char* data = maskL_checklstring(L, 1, &size);

  /* do partial on input */
  size_t v3 = 0;
  uint32_t result = 0; /* initial = 0 */
  int v5 = 0;
  for (; v3 < size; result = ((uint32_t)(1025 * (v5 + result)) >> 6) ^ (1025 * (v5 + result))) {
    v5 = data[v3++];
  }

  /* finalise */
  result = (0x8001 * (((uint32_t)(9 * result) >> 11) ^ (9 * result)));

  /* done, give result */
  mask_pushinteger(L, result);
  return 1;
}


// Times33, a.k.a DJBX33A is the hashing algorithm used in PHP's hash table.
static int times33(mask_State *L)
{
  const std::string str = maskL_checkstring(L, 1);
  unsigned long hash = 0;
 
  for (auto c : str)
  {
    hash = (hash * 33) ^ (unsigned long)c;
  }
  
  mask_pushinteger(L, hash);
  return 1;
}


static int murmur1(mask_State *L)
{
  size_t textLen;
  const auto text = maskL_checklstring(L, 1, &textLen);
  const auto seed = (unsigned int)maskL_optinteger(L, 2, 0);
  const auto hash = MurmurHash1Aligned(text, (int)textLen, seed);
  mask_pushinteger(L, hash);
  return 1;
}

// P1: String
// P2: Seed
// P3: Boolean whether to follow alignment.
static int murmur2(mask_State *L)
{
  size_t textLen;
  const auto text = maskL_checklstring(L, 1, &textLen);
  const auto seed = maskL_optinteger(L, 2, 0);
  unsigned int hash;

  if (mask_toboolean(L, 3))
    hash = MurmurHashAligned2(text, (int)textLen, (uint32_t)seed);
  else
    hash = MurmurHash2(text, (int)textLen, (uint32_t)seed);

  mask_pushinteger(L, hash);
  return 1;
}


// x86_64
static int murmur64a(mask_State *L)
{
  size_t textLen;
  const auto text = maskL_checklstring(L, 1, &textLen);
  const auto seed = (uint64_t)maskL_optinteger(L, 2, 0);
  const auto hash = MurmurHash64A(text, (int)textLen, seed);
  mask_pushinteger(L, hash);
  return 1;
}


// 64-bit hash for 32-bit platforms
static int murmur64b(mask_State *L)
{
  size_t textLen;
  const auto text = maskL_checklstring(L, 1, &textLen);
  const auto seed = (uint64_t)maskL_optinteger(L, 2, 0);
  const auto hash = MurmurHash64B(text, (int)textLen, seed);
  mask_pushinteger(L, hash);
  return 1;
}


static int murmur2a(mask_State *L)
{
  size_t textLen;
  const auto text = maskL_checklstring(L, 1, &textLen);
  const auto seed = (uint32_t)maskL_optinteger(L, 2, 0);
  const auto hash = MurmurHash2A(text, (int)textLen, seed);
  mask_pushinteger(L, hash);
  return 1;
}


// Architecture independent.
static int murmur2neutral(mask_State *L)
{
  size_t textLen;
  const auto text = maskL_checklstring(L, 1, &textLen);
  const auto seed = (uint32_t)maskL_optinteger(L, 2, 0);
  const auto hash = MurmurHashNeutral2(text, (int)textLen, seed);
  mask_pushinteger(L, hash);
  return 1;
}


// Just the name. I still prefer DJB.
static int superfasthash(mask_State *L)
{
  size_t textLen;
  const auto text = maskL_checklstring(L, 1, &textLen);
  const auto hash = SuperFastHash((const signed char*)text, (int)textLen);
  mask_pushinteger(L, hash);
  return 1;
}


static int djb2(mask_State *L)
{
  int c;
  auto str = maskL_checkstring(L, 1);
  unsigned long hash = 5381;

  while ((c = *str++))
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

  mask_pushinteger(L, hash);
  return 1;
}


static int sdbm(mask_State *L)
{
  int c;
  auto str = maskL_checkstring(L, 1);
  unsigned long hash = 0;

  while ((c = *str++))
    hash = c + (hash << 6) + (hash << 16) - hash;

  mask_pushinteger(L, hash);
  return 1;
}


static int md5(mask_State *L)
{
  size_t len;
  unsigned char buffer[16] = {};
  const auto str = maskL_checklstring(L, 1, &len);
  md5_fn((unsigned char*)str, (int)len, buffer);
  
  std::stringstream res {};
  for (int i = 0; i < 16; i++)
  {
    res << std::hex << (int)buffer[i];
  }

  mask_pushstring(L, res.str().c_str());
  return 1;
}


static int lookup3(mask_State *L)
{
  size_t len;
  const auto text = maskL_checklstring(L, 1, &len);
  const auto hash = lookup3_impl(text, (int)len, (uint32_t)maskL_optinteger(L, 2, 0));
  mask_pushinteger(L, hash);
  return 1;
}


static int crc32(mask_State *L)
{
  size_t len;
  const auto text = maskL_checklstring(L, 1, &len);
  const auto hash = crc32_impl(text, (int)len, (uint32_t)maskL_optinteger(L, 2, 0));
  mask_pushinteger(L, hash);
  return 1;
}


// The hashing function used inside Mask. Honorary addition.
// Basically a slightly different DJB2.
static int mask(mask_State *L)
{
  size_t l;
  const auto text = maskL_checklstring(L, 1, &l);
  const auto hash = maskS_hash(text, l, (unsigned int)maskL_optinteger(L, 2, 0));
  mask_pushinteger(L, hash);
  return 1;
}


static int l_sha256(mask_State *L)
{
  size_t l;
  char hex[SHA256_HEX_SIZE];
  const auto text = maskL_checklstring(L, 1, &l);
  sha256_hex(text, l, hex);
  mask_pushstring(L, hex);
  return 1;
}


// This should be fairly secure on systems that employ a randomized device, like /dev/urandom, BCryptGenRandom, etc.
// But, otherwise, it's not secure whatsoever.
static int random(mask_State *L)
{
  std::random_device dev;
#if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__) || defined(_M_X64) || defined(__aarch64__)
  std::mt19937_64 rng(dev());
  std::uniform_int_distribution<std::mt19937_64::result_type> dist(maskL_checkinteger(L, 1), maskL_checkinteger(L, 2));
#else
  std::mt19937 rng(dev());
  std::uniform_int_distribution<std::mt19937::result_type> dist((std::mt19937::result_type)maskL_checkinteger(L, 1), (std::mt19937::result_type)maskL_checkinteger(L, 2));
#endif
  mask_pushinteger(L, dist(rng));
  return 1;
}


static int hexdigest(mask_State *L)
{
  std::stringstream stream;
  stream << "0x";
  stream << std::hex << maskL_checkinteger(L, 1);
  mask_pushstring(L, stream.str().c_str());
  return 1;
}


static const maskL_Reg funcs[] = {
  {"hexdigest", hexdigest},
  {"random", random},
  {"sha256", l_sha256},
  {"mask", mask},
  {"crc32", crc32},
  {"lookup3", lookup3},
  {"md5", md5},
  {"sdbm", sdbm},
  {"djb2", djb2},
  {"superfasthash", superfasthash},
  {"murmur2neutral", murmur2neutral},
  {"murmur64b", murmur64b},
  {"murmur64a", murmur64a},
  {"murmur2a", murmur2a},
  {"murmur2", murmur2},
  {"murmur1", murmur1},
  {"times33", times33},
  {"joaat", joaat},
  {"fnv1a", fnv1a},
  {"fnv1", fnv1},
  {NULL, NULL}  
};


MASKMOD_API int maskopen_crypto(mask_State *L)
{
  maskL_newlib(L, funcs);
  return 1;
}

