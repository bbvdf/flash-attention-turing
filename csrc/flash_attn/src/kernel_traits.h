#pragma once

#include "cute/tensor.hpp"

#include "cutlass/cutlass.h"
#include <cutlass/numeric_conversion.h>
#include <cutlass/numeric_types.h>

using namespace cute;

template<int kHeadDim_, int kBlockM_, int kBlockN_, int kNWarps_, typename elem_type=cutlass::half_t>
struct Flash_kernel_traits {

    // Assume __CUDA_ARCH__ == 750

    using Element = elem_type;

    using ElementAccum = float;
    //using index_t = int64_t;

    using MMA_Atom_Arch = MMA_Atom<SM75_16x8x8_F32F16F16F32_TN>;

    // for hdim = 128, each warp loads 16 rows
    using SmemCopyAtomQ = Copy_Atom<SM75_U32x2_LDSM_N, elem_type>;

    using SmemCopyAtomK = Copy_Atom<SM75_U32x4_LDSM_N, elem_type>;

    using SmemCopyAtomV = Copy_Atom<SM75_U16x8_LDSM_T, elem_type>;

};

template<int kHeadDim_, int kBlockM_, int kBlockN_, int kNWarps_, typename elem_type=cutlass::half_t,
         typename Base=Flash_kernel_traits<kHeadDim_, kBlockM_, kBlockN_, kNWarps_, elem_type> >
struct Flash_fwd_kernel_traits : public Base {

    using Element = typename Base::Element;
    using ElementAccum = typename Base::ElementAccum;
    //using index_t = typename Base::index_t;
    using SmemCopyAtomQ = typename Base::SmemCopyAtomQ;
    using SmemCopyAtomK = typename Base::SmemCopyAtomK;
    using SmemCopyAtomV = typename Base::SmemCopyAtomV;

    static constexpr int kBlockM = kBlockM_;
    static constexpr int kBlockN = kBlockN_;
    static constexpr int kHeadDim = kHeadDim_;

    using TiledMma = TiledMMA<
        typename Base::MMA_Atom_Arch,
        Layout<Shape<_8,_1,_1>>,
        // LDSM loads 32 rows for K and V
        Tile<_128, _32, _8>>;

    using SmemLayoutAtomQK = decltype(
        composition(Swizzle<3, 3, 3>{},
                    Layout<Shape<_16, _64>,
                    Stride<_64, _1>>{}));

    using SmemLayoutQ = decltype(tile_to_shape(
        SmemLayoutAtomQK{},
        Shape<Int<kBlockM>, Int<kHeadDim>>{}));


    using SmemLayoutK = decltype(tile_to_shape(
        SmemLayoutAtomQK{},
        Shape<Int<kBlockN>, Int<kHeadDim>>{}));


    using SmemLayoutAtomV = decltype(
        composition(Swizzle<3, 3, 3>{},
                    Layout<Shape<_64, _16>,
                    Stride<_1, _64>>{}));

    using SmemLayoutV = decltype(tile_to_shape(
        SmemLayoutAtomV{},
        Shape<Int<kHeadDim>, Int<kBlockN>>{}));


    using Gmem_copy_struct = AutoVectorizingCopyWithAssumedAlignment<128>;

    using GmemLayoutAtomQK = Layout<Shape <_32, _8>, Stride<_8, _1>>;

    using GmemLayoutAtomV = Layout<Shape <_8, _32>, Stride<_1, _8>>;

    using GmemLayoutAtomO = Layout<Shape <_32, _8>, Stride<_8, _1>>;


    using GmemTiledCopyQK = decltype(
            make_tiled_copy(Copy_Atom<Gmem_copy_struct, Element>{},
                            GmemLayoutAtomQK{},
                            Layout<Shape<_1, _8>>{}));  // Val layout, 8 vals per read

    using GmemTiledCopyV = decltype(
            make_tiled_copy(Copy_Atom<Gmem_copy_struct, Element>{},
                            GmemLayoutAtomV{},
                            Layout<Shape<_8, _1>>{}));  // Val layout, 8 vals per read

    using GmemTiledCopyO = decltype(
            make_tiled_copy(Copy_Atom<Gmem_copy_struct, Element>{},
                            GmemLayoutAtomO{},
                            Layout<Shape<_1, _8>>{}));  // Val layout, 8 vals per read


};
