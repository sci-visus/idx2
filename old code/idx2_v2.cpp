/*
In version 2, we add the capability to store a min-max tree, so that range queries are supported.
For v2.0: the min-max tree is stored at full precision, with no compression.
For v2.1: we compress the min-max tree hierarchically by bit planes:
    - For each brick, we still store [min, max] uncompressed.
    - Then, the range [min, max] is iteratively divided into [min, mid] and [mid, max]
    - For the blocks within the brick, we write 1 if the block intersects with [min, mid]
    - If block B intersects with [min, mid], it may or may not intersect with [mid, max], so we need another bit to encode that information
        - However, if block B does not intersect with [min, mid], it surely intersects with [mid, max], so no bit is needed
    - We treat each block B as a "particle" and encode the list of blocks intersecting [min, mid] using the particle compression scheme
    - For the next bit plane: if block B intersects [min, mid], then it may or may not intersect [min, (min+mid)/2]
        - However, if B does not intersect [min, mid], it surely does not intersect with [min, (min+mid)/2]

We store the min-max tree independently of the main hierarchy, similarly to how we store the exponent hierarchy.        
*/

#include "idx2_common.h"
#include "idx2_filesystem.h"
#include "idx2_function.h"
#include "idx2_volume.h"
#include "idx2_v1.h"
#include "idx2_zfp.h"

namespace idx2 {

static file_id
ConstructFilePathV2(const idx2_file& Idx2, u64 Brick, i8 Level, i8 SubLevel, i16 BitPlane) {
}

// TODO: merge the sublevels into a single file?
static file_id
ConstructFilePathExponentsV2(const idx2_file& Idx2, u64 Brick, i8 Level, i8 SubLevel) {
}

// TODO: we will compute a naive min-max tree first to use as a baseline
// TODO: we need a file path to store the min-max tree
// TODO: figure out exactly how Idx2.FileDirDepths are computed
// TODO: figure out whether bricks-per-file are dependent on the level?
// TODO: to iterate through the blocks inside a brick, we can use the fast stack algorithm
// TODO: we build a min-max tree for each bit plane, and keep the tree for previous bit plane around to traverse in tandems
// TODO: keep the tree in a linear buffer (that can grow by itself), using 16-bit as index for each node (we need to chekc

} // namespace idx2

