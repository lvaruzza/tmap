# TMAP - torrent mapping alignment program

##  General Notes 

TMAP is a fast and accurate alignment software for short and long nucleotide sequences produced by next-generation sequencing technologies.

* The latest TMAP is unsupported.  To use a supported version, please see the TMAP version associated with a Torrent Suite release below.

*  Get the latest source code: 
    <pre lang="bash"><code>git clone git://github.com/iontorrent/TMAP.git
    cd TMAP
    git submodule init
    git submodule update
    </code></pre>
* To download a specific version, please get the latest source code, and then checkout the specific version using the tag listed below:
    <pre lang="bash"><code>git checkout -b tag "tag name below"</code></pre>
    For example: <pre lang="bash"><code>git checkout -b 3.0.1 tmap.3.0.1
    git submodule update
    </code></pre>
    Below is the list of tags associated with a specific Torrent Suite release:<table>
    <tr><td>Torrent Suite 3.0:</td><td>tmap.3.0.1</td></tr>
    <tr><td>Torrent Suite 2.2</td><td>tmap.0.3.7</td></tr>
    <tr><td>Torrent Suite 2.0.1</td><td>tmap.0.2.3</td></tr>
    <tr><td>Torrent Suite 2.0</td><td>tmap.0.2.3</td></tr>
    <tr><td>Torrent Suite 1.5.1</td><td>tmap.0.1.3</td></tr>
    <tr><td>Torrent Suite 1.5</td><td>tmap.0.1.3</td></tr>
    <tr><td>Torrent Suite 1.4.1</td><td>tmap.0.0.28</td></tr>
    <tr><td>Torrent Suite 1.4</td><td>tmap.0.0.25</td></tr>
    <tr><td>Torrent Suite 1.3</td><td>tmap.0.0.19</td></tr>
    <tr><td>Torrent Suite 1.2</td><td>tmap.0.0.9</td></tr>
    </tr>
    </table>
*  See the latest manual: http://github.com/iontorrent/TMAP/blob/master/doc/tmap-book.pdf


##  Pre-requisites
1. Compiler (required):
  The compiler and system must support SSE2 instructions.  

##  To Install

1. Compile TMAP:
  <pre lang="bash"><code>sh autogen.sh && ./configure && make</code></pre>
2. Install
  <pre lang="bash"><code>make install</code></pre>

##  Optional Installs

### TCMalloc (optional)
  TMAP will run approximately 15% faster using the tcmalloc memory allocation
  implementation.  To use tcmalloc, install the Google performance tools:
  http://code.google.com/p/google-perftools
  
  If you have previously compiled TMAP, execute the following command:
  <pre lang="bash"><code>make distclean && sh autogen.sh && ./configure && make clean && make</code></pre>
  After installation, execute the following command:
  <pre lang="bash"><code>sh autogen.sh && ./configure && make clean && make</code></pre>
  The performance improve should occur when using multiple-threads.

##  Developer Notes

There are a number of areas for potential improvement within TMAP for those
that are interested; they will be mentioned here.  A great way to find places
where the code can be improved is to use Google's performance tools:
  http://code.google.com/p/google-perftools
This includes a heap checker, heap profiler, and cpu profiler.  Examining 
performance on large genomes (hg19) is recommended.

### Smith Waterman extensions
  Currently, each hit is examined with Smith Waterman (score only), which
   re-considers the portion of the read that matched during seeding.  We need
   only re-examine the portion of the read that is not matched during seeding.
   This could be tracked during seeding for the Smith Waterman step, though 
   the merging of hits from each algorithm could be complicated by this step.
   Nonetheless, this would improve the run time of the program, especially for
   high-quality data and/or longer reads (>200bp).

### Smith Waterman vectorization
  The vectorized (SSE2) Smith Waterman implemented supports an combination of
    start and end soft-clipping.  To support any type of soft-clipping, some 
    performance trade-offs needed to be made.  In particular, 16-bit integers
	are stored in the 128-bit integers, giving only 8 bytes/values per 128-bit 
    integer.  This could be improved to 16 bytes/values per 128-bit integer by
    using 8-bit integers.  This would require better overflow handling.  Also,
    handling negative infinity for the Smith Waterman initialization would be
    difficult.  Nonetheless, this could significantly improve the performance of
    the most expensive portion of the program.

### Best two-stage mapping
  There is no current recommendation for the best settings for two-stage 
    mapping, which could significantly decrease the running time.  A good 
	project would be to optimize these settings.

### Mapping quality calibration
  The mapping quality is sufficiently calibrated, but can always be improved,
    especially for longer reads.  This is a major area for improvement.

### Better support for paired ends/mate pairs
  There is minimal support for paired ends/mate pairs, which relies on knowing
    a prior the parameters for the insert size distribution.  The insert size 
	could be trained on a subset of the given input data.

### Speeding up lookups in the FM-index/BWT.
  Further implementation improvements or parameter tuning could be made to make
    the lookups in the FM-index/BWT faster.  This includes the occurrence 
	interval, the suffix array interval, and the k-mer occurence hash.  Caching
	these results may also make sense when examining the same sub-strings across
	multiple algorithms.  Speed improvements have already been made to BWA and 
	could be relevant here:
	  http://github.com/RoelKluin/bwa

### Dynamic split read mapping
  It is important to detect Structural Variation (SV), as well as finding splice 
    junctions for RNA-seq.  Support for returning more than one alignment, where
	these alignments do not significantly overlap in terms of which bases they
	consume in the query, could be included.  For example, a 400bp read could span
	a SV breakpiont, with the first 100bp on one side of the breakpoint and the 
	second 300bp on the other.  Currently (with full soft-clipping options turned 
    on), we may produce two alignments for the two parts of the query. Nonetheless,
	the "choice" algorithm will choose the one with the best alignment score 
    (typically the 300bp one), and so only one alignment will be present in the SAM
	file.  A better strategy would be to search for pairs (triples, etc.) of 
	alignments that do not significantly overlap in the query (i.e. consume the same
	query bases).  This would directly find SVs as well as other types of variant
	requiring split read mapping.

### Representative repetitive hits
  If a seeding algorithm finds a large occurence interval that it will save, it 
    could save one of the occurrences (random) as a representative hit for the 
	repetitive interval.  This representative hit could be aligned with Smith-Waterman
	and its alignment score could be compared to the other hits.  If its score is
	better, than the read could be flagged as repetitive and "unmapped".  The 
	algorithm would need to be careful that the repetitive hit is not contained 
	within the returned non-repeititve hits, as to cause many reads to be unmapped.
