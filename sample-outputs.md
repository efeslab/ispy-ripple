# Finagle-chirper

A sample execution's input and output of Finagle-chirper is listed below:

```
# Run command
$ java -jar renaissance.jar finagle-chirper -r 1
# output
WARNING: An illegal reflective access operation has occurred
WARNING: Illegal reflective access by com.twitter.jvm.Hotspot (file:/mnt/storage/takh/experiments/renaissance/./tmp-jars-12569418405445365198/cp-util-jvm_2.11-19.4.0.jar3264199144317961462.jar) to field sun.management.ManagementFactoryHelper.jvm
WARNING: Please consider reporting this to the maintainers of com.twitter.jvm.Hotspot
WARNING: Use --illegal-access=warn to enable warnings of further illegal reflective access operations
WARNING: All illegal access operations will be denied in a future release
SLF4J: Failed to load class "org.slf4j.impl.StaticLoggerBinder".
SLF4J: Defaulting to no-operation (NOP) logger implementation
SLF4J: See http://www.slf4j.org/codes.html#StaticLoggerBinder for further details.
Aug 05, 2022 12:02:28 PM com.twitter.finagle.Init$$anonfun$6 apply$mcV$sp
INFO: Finagle version 19.4.0 (rev=15ae0aba979a2c11ed4a71774b2e995f5df918b4) built at 20190418-114039
Master port: 39055
Cache ports: 41385, 40561, 33245, 37443, 39427, 35659, 33247, 35627, 44613, 34809, 38747, 37395, 34929, 36547, 40219, 46095, 46081, 35639, 33587, 39187, 39553, 37845, 41055, 35307, 44181, 42623, 42457, 44367, 40305, 39751, 44109, 41723, 40117, 33577, 39273, 44525, 35353, 41451, 45729, 42889, 39875, 36533, 45291, 44117, 40135, 33943, 44315, 34389, 39095, 35063, 34577, 36831, 44125, 45833, 45547, 34269, 38547, 42829, 40249, 43057, 33099, 41741, 41909, 46719, 40113, 36809, 36443, 40147, 36495, 34563, 40243, 37579, 38619, 41177, 41183, 40827, 37399, 33811, 42021, 44177, 36477, 35661, 37783, 40077, 41727, 33141, 42651, 43067, 37387, 37371, 45513, 36843, 39441, 36965, 46397, 45645, 44201, 35257, 38901, 37067, 34633, 40355, 40453, 44715
====== finagle-chirper (twitter-finagle) [default], iteration 0 started ======
Resetting master, feed map size: 5000
GC before operation: completed in 27.133 ms, heap usage 53.350 MB -> 33.560 MB.
WARNING: This benchmark provides no result that can be validated.
         There is no way to check that no silent failure occurred.
====== finagle-chirper (twitter-finagle) [default], iteration 0 completed (26866.728 ms) ======
```

# Finagle-HTTP

A sample execution's input and output of Finagle-HTTP is listed below:

```
# Run command
$ java -jar renaissance.jar finagle-http -r 1
# output
WARNING: An illegal reflective access operation has occurred
WARNING: Illegal reflective access by com.twitter.jvm.Hotspot (file:/mnt/storage/takh/experiments/renaissance/./tmp-jars-6071431901685454737/cp-util-jvm_2.11-19.4.0.jar7840337106309187469.jar) to field sun.management.ManagementFactoryHelper.jvm
WARNING: Please consider reporting this to the maintainers of com.twitter.jvm.Hotspot
WARNING: Use --illegal-access=warn to enable warnings of further illegal reflective access operations
WARNING: All illegal access operations will be denied in a future release
SLF4J: Failed to load class "org.slf4j.impl.StaticLoggerBinder".
SLF4J: Defaulting to no-operation (NOP) logger implementation
SLF4J: See http://www.slf4j.org/codes.html#StaticLoggerBinder for further details.
Aug 05, 2022 12:05:12 PM com.twitter.finagle.Init$$anonfun$6 apply$mcV$sp
INFO: Finagle version 19.4.0 (rev=15ae0aba979a2c11ed4a71774b2e995f5df918b4) built at 20190418-114039
finagle-http on :42079 spawning 104 client and default number of server workers.
====== finagle-http (twitter-finagle) [default], iteration 0 started ======
GC before operation: completed in 24.932 ms, heap usage 22.585 MB -> 19.199 MB.
====== finagle-http (twitter-finagle) [default], iteration 0 completed (10478.827 ms) ======
```

# Cassandra

A sample execution's input and output of Cassandra is listed below:

```
# Run command
$ java -jar dacapo.jar cassandra -n 1
# output
--------------------------------------------------------------------------------
IMPORTANT NOTICE:  This is NOT a release build of the DaCapo suite.
Since it is not an official release of the DaCapo suite, care must be taken when
using the suite, and any use of the build must be sure to note that it is not an
offical release, and should note the relevant git hash.

Feedback is greatly appreciated.   The preferred mode of feedback is via github.
Please use our github page to create an issue or a pull request.
    https://github.com/dacapobench/dacapobench.
--------------------------------------------------------------------------------

Using scaled threading model. 104 processors detected, 104 threads used to drive the workload, in a possible range of [1,unlimited]
Cassandra starting...
SLF4J: Class path contains multiple SLF4J bindings.
SLF4J: Found binding in [jar:file:/mnt/storage/takh/experiments/dacapo/./scratch/jar/logback-classic-1.1.3.jar!/org/slf4j/impl/StaticLoggerBinder.class]
SLF4J: Found binding in [jar:file:/mnt/storage/takh/experiments/dacapo/./scratch/jar/slf4j-simple-1.7.25.jar!/org/slf4j/impl/StaticLoggerBinder.class]
SLF4J: See http://www.slf4j.org/codes.html#multiple_bindings for an explanation.
SLF4J: Actual binding is of type [ch.qos.logback.classic.util.ContextSelectorStaticBinder]
WARNING: An illegal reflective access operation has occurred
WARNING: Illegal reflective access by org.apache.cassandra.utils.FBUtilities (file:/mnt/storage/takh/experiments/dacapo/./scratch/jar/cassandra-3.11.3.jar) to field java.io.FileDescriptor.fd
WARNING: Please consider reporting this to the maintainers of org.apache.cassandra.utils.FBUtilities
WARNING: Use --illegal-access=warn to enable warnings of further illegal reflective access operations
WARNING: All illegal access operations will be denied in a future release
YCSB starting...
===== DaCapo evaluation-git+309e1fa cassandra starting =====
Iteration Started...
Command line: -db com.yahoo.ycsb.db.CassandraCQLClient -p hosts=localhost -P ./scratch/ycsb-workloads/workloada -load
YCSB Client 0.15.0

Loading workload...
Starting test.
DBWrapper: report latency for each error is false and specific error codes to track for latency are: []
Command line: -db com.yahoo.ycsb.db.CassandraCQLClient -p hosts=localhost -P ./scratch/ycsb-workloads/workloada -t
YCSB Client 0.15.0

Loading workload...
Starting test.
DBWrapper: report latency for each error is false and specific error codes to track for latency are: []
Iteration Finished...
===== DaCapo evaluation-git+309e1fa cassandra PASSED in 5942 msec =====
```

# Kafka

A sample execution's input and output of Kafka is listed below:

```
# Run command
$ java -jar dacapo.jar kafka -n 1
# output
--------------------------------------------------------------------------------
IMPORTANT NOTICE:  This is NOT a release build of the DaCapo suite.
Since it is not an official release of the DaCapo suite, care must be taken when
using the suite, and any use of the build must be sure to note that it is not an
offical release, and should note the relevant git hash.

Feedback is greatly appreciated.   The preferred mode of feedback is via github.
Please use our github page to create an issue or a pull request.
    https://github.com/dacapobench/dacapobench.
--------------------------------------------------------------------------------

Using scaled threading model. 104 processors detected, 104 threads used to drive the workload, in a possible range of [1,unlimited]
Starting Zookeeper...
Starting Kafka Server...
[2022-08-05 12:08:56,721] WARN No meta.properties file under dir /tmp/kafka-logs/meta.properties (kafka.server.BrokerMetadataCheckpoint)
[2022-08-05 12:08:56,847] WARN Unable to read additional data from client sessionid 0x0, likely client has closed socket (org.apache.zookeeper.server.NIOServerCnxn)
Starting Agent...
[2022-08-05 12:08:57,184] WARN No meta.properties file under dir /tmp/kafka-logs/meta.properties (kafka.server.BrokerMetadataCheckpoint)
Starting Coordinator...
===== DaCapo evaluation-git+309e1fa kafka starting =====
Trogdor is running the benchmark....
Finished
===== DaCapo evaluation-git+309e1fa kafka PASSED in 6243 msec =====
[2022-08-05 12:09:04,200] WARN Running coordinator shutdown hook. (org.apache.kafka.trogdor.coordinator.Coordinator)
[2022-08-05 12:09:04,201] WARN Running agent shutdown hook. (org.apache.kafka.trogdor.agent.Agent)
Shutdown Agent...
Shutdown Coordinator...
```

# Tomcat

A sample execution's input and output of Tomcat is listed below:

```
# Run command
$ java -jar dacapo.jar tomcat -n 1
# output
--------------------------------------------------------------------------------
IMPORTANT NOTICE:  This is NOT a release build of the DaCapo suite.
Since it is not an official release of the DaCapo suite, care must be taken when
using the suite, and any use of the build must be sure to note that it is not an
offical release, and should note the relevant git hash.

Feedback is greatly appreciated.   The preferred mode of feedback is via github.
Please use our github page to create an issue or a pull request.
    https://github.com/dacapobench/dacapobench.
--------------------------------------------------------------------------------

Using scaled threading model. 104 processors detected, 104 threads used to drive the workload, in a possible range of [1,1024]
Server thread created
===== DaCapo evaluation-git+309e1fa tomcat starting =====
Loading web application
Creating client threads
Waiting for clients to complete
Client threads complete ... unloading web application
WARNING: An illegal reflective access operation has occurred
WARNING: Illegal reflective access by org.apache.catalina.loader.WebappClassLoaderBase (file:/mnt/storage/takh/experiments/dacapo/scratch/lib/catalina.jar) to field java.io.ObjectStreamClass$Caches.localDescs
WARNING: Please consider reporting this to the maintainers of org.apache.catalina.loader.WebappClassLoaderBase
WARNING: Use --illegal-access=warn to enable warnings of further illegal reflective access operations
WARNING: All illegal access operations will be denied in a future release
===== DaCapo evaluation-git+309e1fa tomcat PASSED in 4300 msec =====
Server stopped ... iteration complete
```

# Verilator

A sample execution's input and output of Verilator is listed below:

```
# Run command
$ ./rocket-chip/emulator/emulator-freechips.rocketchip.system-DefaultConfig-dynamic +cycle-count ./rocket-chip/emulator/dhrystone-head.riscv
# output
This emulator compiled with JTAG Remote Bitbang client. To enable, use +jtag_rbb_enable=1.
Listening on port 45237
Microseconds for one run through Dhrystone: 495
Dhrystones per Second:                      2016
mcycle = 248002
minstret = 198783
Completed after 491664 cycles
```
