#define lhashlib_c
#define HELLO_LIB

#include "hello.h"
#include "hellolib.h"
#include "lprefix.h"
#include "helloconf.h"
#include "lauxlib.h"
#include "lstring.h"
#include "lcryptolib.hpp"

#include <ios>
#include <string>
#include <random>
#include <sstream>
#include <iomanip>


static int fnv1(hello_State *L)
{
  const auto FNV_offset_basis = 0xcbf29ce484222325;
  const auto FNV_prime = 0x100000001b3;
  const std::string s  = helloL_checkstring(L, 1);
  hello_Integer hash     = FNV_offset_basis;
  
  for (auto& c : s)
  {
    hash *= FNV_prime;
    hash ^= c;
  }

  hello_pushinteger(L, hash);
  return 1;
}


static int fnv1a(hello_State *L)
{
  const auto FNV_offset_basis = 0xcbf29ce484222325;
  const auto FNV_prime = 0x100000001b3;
  const std::string s  = helloL_checkstring(L, 1);
  hello_Integer hash     = FNV_offset_basis;
  
  for (auto& c : s)
  {
    hash ^= c;
    hash *= FNV_prime;
  }

  hello_pushinteger(L, hash);
  return 1;
}


static int joaat(hello_State *L)
{
  /* get input */
  size_t size;
  const char* data = helloL_checklstring(L, 1, &size);

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
  hello_pushinteger(L, result);
  return 1;
}


// Times33, a.k.a DJBX33A is the hashing algorithm used in PHP's hash table.
static int times33(hello_State *L)
{
  const std::string str = helloL_checkstring(L, 1);
  unsigned long hash = 0;
 
  for (auto c : str)
  {
    hash = (hash * 33) ^ (unsigned long)c;
  }
  
  hello_pushinteger(L, hash);
  return 1;
}


static int murmur1(hello_State *L)
{
  size_t textLen;
  const auto text = helloL_checklstring(L, 1, &textLen);
  const auto seed = (unsigned int)helloL_optinteger(L, 2, 0);
  const auto hash = MurmurHash1Aligned(text, (int)textLen, seed);
  hello_pushinteger(L, hash);
  return 1;
}

// P1: String
// P2: Seed
// P3: Boolean whether to follow alignment.
static int murmur2(hello_State *L)
{
  size_t textLen;
  const auto text = helloL_checklstring(L, 1, &textLen);
  const auto seed = helloL_optinteger(L, 2, 0);
  unsigned int hash;

  if (hello_toboolean(L, 3))
    hash = MurmurHashAligned2(text, (int)textLen, (uint32_t)seed);
  else
    hash = MurmurHash2(text, (int)textLen, (uint32_t)seed);

  hello_pushinteger(L, hash);
  return 1;
}


// x86_64
static int murmur64a(hello_State *L)
{
  size_t textLen;
  const auto text = helloL_checklstring(L, 1, &textLen);
  const auto seed = (uint64_t)helloL_optinteger(L, 2, 0);
  const auto hash = MurmurHash64A(text, (int)textLen, seed);
  hello_pushinteger(L, hash);
  return 1;
}


// 64-bit hash for 32-bit platforms
static int murmur64b(hello_State *L)
{
  size_t textLen;
  const auto text = helloL_checklstring(L, 1, &textLen);
  const auto seed = (uint64_t)helloL_optinteger(L, 2, 0);
  const auto hash = MurmurHash64B(text, (int)textLen, seed);
  hello_pushinteger(L, hash);
  return 1;
}


static int murmur2a(hello_State *L)
{
  size_t textLen;
  const auto text = helloL_checklstring(L, 1, &textLen);
  const auto seed = (uint32_t)helloL_optinteger(L, 2, 0);
  const auto hash = MurmurHash2A(text, (int)textLen, seed);
  hello_pushinteger(L, hash);
  return 1;
}


// Architecture independent.
static int murmur2neutral(hello_State *L)
{
  size_t textLen;
  const auto text = helloL_checklstring(L, 1, &textLen);
  const auto seed = (uint32_t)helloL_optinteger(L, 2, 0);
  const auto hash = MurmurHashNeutral2(text, (int)textLen, seed);
  hello_pushinteger(L, hash);
  return 1;
}


// Just the name. I still prefer DJB.
static int superfasthash(hello_State *L)
{
  size_t textLen;
  const auto text = helloL_checklstring(L, 1, &textLen);
  const auto hash = SuperFastHash((const signed char*)text, (int)textLen);
  hello_pushinteger(L, hash);
  return 1;
}


static int djb2(hello_State *L)
{
  int c;
  auto str = helloL_checkstring(L, 1);
  unsigned long hash = 5381;

  while ((c = *str++))
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

  hello_pushinteger(L, hash);
  return 1;
}


static int sdbm(hello_State *L)
{
  int c;
  auto str = helloL_checkstring(L, 1);
  unsigned long hash = 0;

  while ((c = *str++))
    hash = c + (hash << 6) + (hash << 16) - hash;

  hello_pushinteger(L, hash);
  return 1;
}


static int md5(hello_State *L)
{
  size_t len;
  unsigned char buffer[16] = {};
  const auto str = helloL_checklstring(L, 1, &len);
  md5_fn((unsigned char*)str, (int)len, buffer);
  
  std::stringstream res {};
  for (int i = 0; i < 16; i++)
  {
    res << std::hex << (int)buffer[i];
  }

  hello_pushstring(L, res.str().c_str());
  return 1;
}


static int lookup3(hello_State *L)
{
  size_t len;
  const auto text = helloL_checklstring(L, 1, &len);
  const auto hash = lookup3_impl(text, (int)len, (uint32_t)helloL_optinteger(L, 2, 0));
  hello_pushinteger(L, hash);
  return 1;
}


static int crc32(hello_State *L)
{
  size_t len;
  const auto text = helloL_checklstring(L, 1, &len);
  const auto hash = crc32_impl(text, (int)len, (uint32_t)helloL_optinteger(L, 2, 0));
  hello_pushinteger(L, hash);
  return 1;
}


// The hashing function used inside Hello. Honorary addition.
// Basically a slightly different DJB2.
static int hello(hello_State *L)
{
  size_t l;
  const auto text = helloL_checklstring(L, 1, &l);
  const auto hash = helloS_hash(text, l, (unsigned int)helloL_optinteger(L, 2, 0));
  hello_pushinteger(L, hash);
  return 1;
}


static int l_sha256(hello_State *L)
{
  size_t l;
  char hex[SHA256_HEX_SIZE];
  const auto text = helloL_checklstring(L, 1, &l);
  sha256_hex(text, l, hex);
  hello_pushstring(L, hex);
  return 1;
}


// This should be fairly secure on systems that employ a randomized device, like /dev/urandom, BCryptGenRandom, etc.
// But, otherwise, it's not secure whatsoever.
static int random(hello_State *L)
{
  std::random_device dev;
#if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__) || defined(_M_X64) || defined(__aarch64__)
  std::mt19937_64 rng(dev());
  std::uniform_int_distribution<std::mt19937_64::result_type> dist(helloL_checkinteger(L, 1), helloL_checkinteger(L, 2));
#else
  std::mt19937 rng(dev());
  std::uniform_int_distribution<std::mt19937::result_type> dist((std::mt19937::result_type)helloL_checkinteger(L, 1), (std::mt19937::result_type)helloL_checkinteger(L, 2));
#endif
  hello_pushinteger(L, dist(rng));
  return 1;
}


static int hexdigest(hello_State *L)
{
  std::stringstream stream;
  stream << "0x";
  stream << std::hex << helloL_checkinteger(L, 1);
  hello_pushstring(L, stream.str().c_str());
  return 1;
}


static const helloL_Reg funcs[] = {
  {"hexdigest", hexdigest},
  {"random", random},
  {"sha256", l_sha256},
  {"hello", hello},
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


HELLOMOD_API int helloopen_crypto(hello_State *L)
{
  helloL_newlib(L, funcs);
  return 1;
}

