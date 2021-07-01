# ispy-ripple

Data center application traces used for ispy and ripple can be downloaded from [here](https://drive.google.com/file/d/1tN8Jw1TcZ9CrDzDWK0HFUD-nVLhZDW9e/view?usp=sharing). These traces can be simulated using the [Intel PT frontend of zsim](https://github.com/stanford-mast/zsim/tree/memtrace).

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

We also provide a virtualbox image where HHVM workloads are pre-installed and set up to run. Please download the VMDK file from [here](https://drive.google.com/file/d/1pP85BDT7wm4--o6NUMMwBqBej6CFKBso/view?usp=sharing). For instructions on how to import VMDK files, see this (blog)[https://medium.com/riow/how-to-open-a-vmdk-file-in-virtualbox-e1f711deacc4]. The oss-performance benchmarks are in a folder of the same name that you can cd into right from `~`. You'll have to run the one line shell command from time_wait.md before you get started running benchmarks (if you don't it will tell you to do so). Also, the first run of a benchmark on a given startup will want to login to mysql to set things up- the credentials are `root` and `root`. So, to run the wordpress benchmark, for example, you would boot up the machine, cd into `oss-performance`, `cat time_wait.md` to figure out the one-liner shell command to run, then run `hhvm perf.php --wordpress --hhvm=$(which hhvm)`.

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


More information about setting up verilator and virtual machine images containing workloads are coming soon.

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
