# ispy-ripple

We provide a virtualbox image where data center applications used for i-spy and ripple evaluation are pre-installed and set up to run. Please download the VMDK file from [here](https://drive.google.com/file/d/1pP85BDT7wm4--o6NUMMwBqBej6CFKBso/view?usp=sharing). For instructions on how to import VMDK files, see this [blog](https://medium.com/riow/how-to-open-a-vmdk-file-in-virtualbox-e1f711deacc4). We tested this virtualbox image with 8 processor cores and 16GB RAM. The username/password for the virtual machine are `osboxes.org`. Data center application traces used for ispy and ripple can be downloaded from [here](https://drive.google.com/file/d/1tN8Jw1TcZ9CrDzDWK0HFUD-nVLhZDW9e/view?usp=sharing). These traces can be simulated using the [Intel PT frontend of zsim](https://github.com/stanford-mast/zsim/tree/memtrace).

# Setting up workloads on host/guest environment

This guide assumes that you are using ubuntu Linux distribution.

Three HHVM applications (Drupal, Mediawiki, Wordpress): https://github.com/facebookarchive/oss-performance

Once you have installed HHVM (we use HHVM 5 built with gcc, the default ubuntu software manager one did not work for us, we had to follow this guide, https://docs.hhvm.com/hhvm/installation/building-from-source#ubuntu-15.04-vivid) and oss-performance suite along with their prerequisites, you can run the benchmarks using the following commands:

```
export HHVM=/path/to/hhvm
pushd .
cd /path/to/oss-performance
for i in drupal7 wordpress mediawiki;
do
  $HHVM perf.php --$i --hhvm=$(echo $HHVM)
done
```

Alternatively, you can use the virtualbox image where HHVM workloads are pre-installed and set up to run. The oss-performance benchmarks are in a folder of the same name that you can cd into right from `~`. You'll have to run the one line shell command from time_wait.md before you get started running benchmarks (if you don't it will tell you to do so). Also, the first run of a benchmark on a given startup will want to login to mysql to set things up- the credentials are `root` and `root`. So, to run the wordpress benchmark, for example, you would boot up the machine, cd into `oss-performance`, `cat time_wait.md` to figure out the one-liner shell command to run, then run `hhvm perf.php --wordpress --hhvm=$(which hhvm)`.

Three DaCapo applications (Cassandra, Kafka, Tomcat): http://dacapobench.org

Once you have installed jvm (we used GraalVM, installation guide, https://www.graalvm.org/docs/getting-started-with-graalvm/linux/) and dacapo (We download download the file from https://sourceforge.net/projects/dacapobench/files/evaluation/dacapo-evaluation-git%2B309e1fa-java8.jar/download), you can run the benchmarks using the following commands:

```
for i in cassandra kafka tomcat;
do
  java -jar /path/to/dacapo-evaluation-git+309e1fa-java8.jar $i -n 10
done
```

Two Renaissance applications (Finagle-Chirper and Finagle-HTTP): https://renaissance.dev
Once you have installed jvm (again, we use GraalVM) and renaissance (we download the file from https://github.com/renaissance-benchmarks/renaissance/releases/download/v0.10.0/renaissance-gpl-0.10.0.jar), you can run the benchmarks using the following commands:

```
for i in chirper http;
do
  java -jar /path/to/renaissance.jar finagle-$i -r 10
done
```

# Setting up verilator (originally provided by Scott Beamer)

1) Get the rocket-chip repo
```
git clone https://github.com/freechipsproject/rocket-chip.git
```
2) Move to the same commit I was at:
```
cd rocket-chip
git checkout aca2f0c3b9fb81f42e4d1
```
3) Instantiate the appropriate version of riscv-fesvr
```
git submodule update --init riscv-tools
cd riscv-tools
git submodule update --init riscv-tools riscv-fesvr
```
4) Create a RISCV directory (to hold relevant software)
```
mkdir riscv
export RISCV=path to riscv
```
5) Build & install riscv-fesvr
```
cd rocket-chip/riscv-tools/riscv-fesvr
mkdir build
cd build
../configure --prefix=$RISCV --target=riscv64-unknown-elf
make install
```
6) Download and expand the tarball from Google Drive (https://drive.google.com/file/d/1WnrYhhYTGUMpndvoNpnzJOjgxdTOBHyL/view?usp=sharing) into rocket-chip (overwriting emulator)

Hopefully make will be able to reuse things, or at least choose to not invoke Chisel to regenerate the verilog files. I would try the binaries right away, and progressively try deleting more things (and rebuilding them for your system) until it works. The instructions below are for the single-core simulator because it is the fastest, but once you have a working methodology, change DefaultConfig to DefaultConfigN8 or DefaultConfigN16 to get more cores.

7) Try executing the single-core simulator that is there:
```
cd emulator
./emulator-freechips.rocketchip.system-DefaultConfig +cycle-count ./dhrystone-head.riscv
```
8) (If the above doesn't work), try to encourage make to re-link it all, and try #7 again
```
rm emulator-freechips.rocketchip.system-DefaultConfig
make CONFIG=DefaultConfig
```
9) (If the above doesn't work), try rebuilding all .o files, and try #8 again
```
rm generated-src/freechips.rocketchip.system.DefaultConfig/*.o
```
10) (If the above doesn't work), try rebuilding verilator (and re-running verilator) by executing #8 again
```
rm -rf verilator
rm -rf generated-src/freechips.rocketchip.system.DefaultConfig
```
When you start compiling large binaries, you can turn on parallel builds with -j:
```
make MAKEFLAGS="$MAKEFLAGS -j4" CONFIG=DefaultConfigN8
```
If you do need to run the Scala to generate new designs, make sure to install version 8 of the JVM. If when emitting a large design you run out of memory, you can ask for more:
```
make JVM_MEMORY=8G MAKEFLAGS="$MAKEFLAGS -j4" CONFIG=DefaultConfigN8
```

# Publications

```
@inproceedings{khan2020ispy,
  author = {Khan, Tanvir Ahmed and Sriraman, Akshitha and Devietti, Joseph and Pokam, Gilles and Litz, Heiner and Kasikci, Baris},
  title = {I-SPY: Context-Driven Conditional Instruction Prefetching with Coalescing},
  booktitle = {Proceedings of the 53rd IEEE/ACM International Symposium on Microarchitecture (MICRO)},
  series = {MICRO 2020},
  year = {2020},
  publisher = {IEEE},
  url = {https://doi.org/10.1109/MICRO50266.2020.00024},
  doi = {10.1109/MICRO50266.2020.00024},
}

@inproceedings{khan2021ripple,
  author = {Khan, Tanvir Ahmed and Zhang, Dexin and Sriraman, Akshitha and Devietti, Joseph and Pokam, Gilles and Litz, Heiner and Kasikci, Baris},
  title = {Ripple: Profile-Guided Instruction Cache Replacement for Data Center Applications},
  booktitle = {Proceedings of the 48th International Symposium on Computer Architecture (ISCA)},
  series = {ISCA 2021},
  year = {2021},
  month = jun,
}
```
