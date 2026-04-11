To build the project, simply use the default recipe for makefile in the project repoistory, then run the executable with an id
that indicates to the node/proccess who it is.

example

make // generate driver.out execuable
driver.out 3 // intiates an instance of a node in the system with a label of 3

If you are testing this on dc##.utdallas.edu machines, then you can type make run, and it will build the executable, then strip the id from the hostname of the machine your on and supply that to the driver exetcuable.

For instance suppose you ssh to dc07.utdallas.edu

then make run would

1) generate driver.out
2) "07" would get passed to driver.out, and driver.out would parse the id as 7.

For whatever reason if this doesn't work when testing, simply just use make, then self supply the id as an argument but through my own testing using the DC machines,  "make run" seems to work.

The intented way to test the system for now is to
1) open n terminal instances for n nodes
2) SSH into each respective of the DC machines (order doesn't matter)
3) cd into the project repository, then just use the run recipe (type make run) or
generate driver.out and supply the respective id as mentioned above (type make, then driver.out # for the DC#.utdallas machine)
