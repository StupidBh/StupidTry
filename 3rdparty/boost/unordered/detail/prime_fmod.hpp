// Copyright (C) 2022 Joaquin M Lopez Munoz.
// Copyright (C) 2022-2023 Christian Mazakas
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_UNORDERED_DETAIL_PRIME_FMOD_HPP
#define BOOST_UNORDERED_DETAIL_PRIME_FMOD_HPP

#include <boost/unordered/detail/narrow_cast.hpp>

#include <boost/config.hpp>
#include <boost/cstdint.hpp>

#include <climits>
#include <cstddef>

#if defined(SIZE_MAX)
    #if ((((SIZE_MAX >> 16) >> 16) >> 16) >> 15) != 0
        #define BOOST_UNORDERED_FCA_HAS_64B_SIZE_T
    #endif
#elif defined(UINTPTR_MAX) /* used as proxy for std::size_t */
    #if ((((UINTPTR_MAX >> 16) >> 16) >> 16) >> 15) != 0
        #define BOOST_UNORDERED_FCA_HAS_64B_SIZE_T
    #endif
#endif

#if defined(BOOST_UNORDERED_FCA_HAS_64B_SIZE_T) && defined(_MSC_VER)
    #include <intrin.h>
#endif

namespace boost {
    namespace unordered {
        namespace detail {
            template<class = void>
            struct prime_fmod_size
            {
                constexpr static std::size_t const sizes[] = { 13ul,
                                                               29ul,
                                                               53ul,
                                                               97ul,
                                                               193ul,
                                                               389ul,
                                                               769ul,
                                                               1543ul,
                                                               3079ul,
                                                               6151ul,
                                                               12289ul,
                                                               24593ul,
                                                               49157ul,
                                                               98317ul,
                                                               196613ul,
                                                               393241ul,
                                                               786433ul,
                                                               1'572'869ul,
                                                               3'145'739ul,
                                                               6'291'469ul,
                                                               12'582'917ul,
                                                               25'165'843ul,
                                                               50'331'653ul,
                                                               100'663'319ul,
                                                               201'326'611ul,
                                                               402'653'189ul,
                                                               805'306'457ul,
                                                               1'610'612'741ul,
                                                               3'221'225'473ul,
#if !defined(BOOST_UNORDERED_FCA_HAS_64B_SIZE_T)
                                                               4'294'967'291ul
#else
                                                               6'442'450'939ull,
                                                               12'884'901'893ull,
                                                               25'769'803'751ull,
                                                               51'539'607'551ull,
                                                               103'079'215'111ull,
                                                               206'158'430'209ull,
                                                               412'316'860'441ull,
                                                               824'633'720'831ull,
                                                               1'649'267'441'651ull
#endif
                };

                constexpr static std::size_t const sizes_len = sizeof(sizes) / sizeof(sizes[0]);

#if defined(BOOST_UNORDERED_FCA_HAS_64B_SIZE_T)
                constexpr static boost::uint64_t const inv_sizes32[] = { 1'418'980'313'362'273'202ull,
                                                                         636'094'623'231'363'849ull,
                                                                         348'051'774'975'651'918ull,
                                                                         190'172'619'316'593'316ull,
                                                                         95'578'984'837'873'325ull,
                                                                         47'420'935'922'132'524ull,
                                                                         23'987'963'684'927'896ull,
                                                                         11'955'116'055'547'344ull,
                                                                         5'991'147'799'191'151ull,
                                                                         2'998'982'941'588'287ull,
                                                                         1'501'077'717'772'769ull,
                                                                         750'081'082'979'285ull,
                                                                         375'261'795'343'686ull,
                                                                         187'625'172'388'393ull,
                                                                         93'822'606'204'624ull,
                                                                         46'909'513'691'883ull,
                                                                         23'456'218'233'098ull,
                                                                         11'728'086'747'027ull,
                                                                         5'864'041'509'391ull,
                                                                         2'932'024'948'977ull,
                                                                         1'466'014'921'160ull,
                                                                         733'007'198'436ull,
                                                                         366'503'839'517ull,
                                                                         183'251'896'093ull,
                                                                         91'625'960'335ull,
                                                                         45'812'983'922ull,
                                                                         22'906'489'714ull,
                                                                         11'453'246'088ull,
                                                                         5'726'623'060ull };

                constexpr static std::size_t const inv_sizes32_len = sizeof(inv_sizes32) / sizeof(inv_sizes32[0]);
#endif /* defined(BOOST_UNORDERED_FCA_HAS_64B_SIZE_T) */

                template<std::size_t SizeIndex, std::size_t Size = sizes[SizeIndex]>
                static std::size_t position(std::size_t hash)
                {
                    return hash % Size;
                }

                constexpr static std::size_t (*positions[])(std::size_t) = {
#if !defined(BOOST_UNORDERED_FCA_HAS_64B_SIZE_T)
                    position<0, sizes[0]>,   position<1, sizes[1]>,   position<2, sizes[2]>,   position<3, sizes[3]>,
                    position<4, sizes[4]>,   position<5, sizes[5]>,   position<6, sizes[6]>,   position<7, sizes[7]>,
                    position<8, sizes[8]>,   position<9, sizes[9]>,   position<10, sizes[10]>, position<11, sizes[11]>,
                    position<12, sizes[12]>, position<13, sizes[13]>, position<14, sizes[14]>, position<15, sizes[15]>,
                    position<16, sizes[16]>, position<17, sizes[17]>, position<18, sizes[18]>, position<19, sizes[19]>,
                    position<20, sizes[20]>, position<21, sizes[21]>, position<22, sizes[22]>, position<23, sizes[23]>,
                    position<24, sizes[24]>, position<25, sizes[25]>, position<26, sizes[26]>, position<27, sizes[27]>,
                    position<28, sizes[28]>, position<29, sizes[29]>,
#else
                    position<29, sizes[29]>, position<30, sizes[30]>, position<31, sizes[31]>,
                    position<32, sizes[32]>, position<33, sizes[33]>, position<34, sizes[34]>,
                    position<35, sizes[35]>, position<36, sizes[36]>, position<37, sizes[37]>,
#endif
                };

                static inline std::size_t size_index(std::size_t n)
                {
                    std::size_t i = 0;
                    for (; i < (sizes_len - 1); ++i) {
                        if (sizes[i] >= n) {
                            break;
                        }
                    }
                    return i;
                }

                static inline std::size_t size(std::size_t size_index) { return sizes[size_index]; }

#if defined(BOOST_UNORDERED_FCA_HAS_64B_SIZE_T)
                // We emulate the techniques taken from:
                // Faster Remainder by Direct Computation: Applications to Compilers and
                // Software Libraries
                // https://arxiv.org/abs/1902.01961
                //
                // In essence, use fancy math to directly calculate the remainder (aka
                // modulo) exploiting how compilers transform division
                //

                static inline boost::uint64_t get_remainder(boost::uint64_t fractional, boost::uint32_t d)
                {
    #if defined(_MSC_VER)
                    // use MSVC intrinsics when available to avoid promotion to 128 bits

                    return __umulh(fractional, d);
    #elif defined(BOOST_HAS_INT128)
                    return static_cast<boost::uint64_t>(((boost::uint128_type)fractional * d) >> 64);
    #else
                    // portable implementation in the absence of boost::uint128_type on 64
                    // bits, which happens at least in GCC 4.5 and prior

                    boost::uint64_t r1 = (fractional & UINT32_MAX) * d;
                    boost::uint64_t r2 = (fractional >> 32) * d;
                    r2 += r1 >> 32;
                    return r2 >> 32;
    #endif /* defined(_MSC_VER) */
                }

                static inline boost::uint32_t fast_modulo(boost::uint32_t a, boost::uint64_t M, boost::uint32_t d)
                {
                    boost::uint64_t fractional = M * a;
                    return (boost::uint32_t)(get_remainder(fractional, d));
                }
#endif /* defined(BOOST_UNORDERED_FCA_HAS_64B_SIZE_T) */

                static inline std::size_t position(std::size_t hash, std::size_t size_index)
                {
#if defined(BOOST_UNORDERED_FCA_HAS_64B_SIZE_T)
                    std::size_t sizes_under_32bit = inv_sizes32_len;
                    if (BOOST_LIKELY(size_index < sizes_under_32bit)) {
                        return fast_modulo(
                            narrow_cast<boost::uint32_t>(hash) + narrow_cast<boost::uint32_t>(hash >> 32),
                            inv_sizes32[size_index],
                            boost::uint32_t(sizes[size_index]));
                    }
                    else {
                        return positions[size_index - sizes_under_32bit](hash);
                    }
#else
                    return positions[size_index](hash);
#endif /* defined(BOOST_UNORDERED_FCA_HAS_64B_SIZE_T) */
                }
            }; // prime_fmod_size

#if defined(BOOST_NO_CXX17_INLINE_VARIABLES)
            // https://en.cppreference.com/w/cpp/language/static#Constant_static_members
            // If a const non-inline (since C++17) static data member or a constexpr
            // static data member (since C++11)(until C++17) is odr-used, a definition
            // at namespace scope is still required, but it cannot have an
            // initializer.
            template<class T>
            constexpr std::size_t prime_fmod_size<T>::sizes[];

    #if defined(BOOST_UNORDERED_FCA_HAS_64B_SIZE_T)
            template<class T>
            constexpr boost::uint64_t prime_fmod_size<T>::inv_sizes32[];
    #endif

            template<class T>
            constexpr std::size_t (*prime_fmod_size<T>::positions[])(std::size_t);
#endif
        } // namespace detail
    } // namespace unordered
} // namespace boost

#endif // BOOST_UNORDERED_DETAIL_PRIME_FMOD_HPP
