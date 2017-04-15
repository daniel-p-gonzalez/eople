[![Build Status](https://travis-ci.org/FBMachine/eople.svg)](https://travis-ci.org/FBMachine/eople)
# Eople

## Introduction

Eople is a concurrency oriented programming language designed to make life easier in a world filled with asynchronous communication. Instead of requiring you to juggle locks, or wire up a confusing web of callbacks, Eople takes a more reactive approach that allows you to solve problems in a way that feels natural. Its familiar syntax makes it quick to learn, and its concurrency constructs are seamlessly integrated.

## Features

* Efficient actor-oriented concurrency
* Fast multi-core virtual machine
* Strong static type system with type inference
* Reactive style for async communication
* Easy to learn

## Installing
Currently, building Eople on Linux and Mac OS X is supported. Windows will likely also work, but no guarantees.

#### Dependencies
* cmake (>=2.6)
* g++ (>=4.7) or clang (>=3.3)
* brew (Mac)

### Linux
```bash
sudo apt-get install cmake
git clone https://github.com/FBMachine/eople.git
cd eople && mkdir build && cd build
cmake ..
sudo make install -j8
cd ../examples
eople test_suite.eop Test::main
```

### Mac OS X
```bash
brew install cmake
git clone https://github.com/FBMachine/eople.git
cd eople && mkdir build && cd build
cmake ..
sudo make install -j8
cd ../examples
eople test_suite.eop Test::main
```

## Hello World
Open up your favorite editor, and let's start with something simple. This should look unsurprising to you if you have experience with a language like python, lua, or ruby.

#### hello.eop
```
def main():
    print("Hello, World!")
end
```

To run this example:
```
eople hello.eop
```
```
Hello, World!
```

For verbose output from the eople runtime, run with -v:
```
eople -v hello.eop
```
```
vm> Initializing with 8 cores.
eve> Importing module 'hello.eop'.
eve> Total time for import: 0.123000 ms.
eve> Import of 'hello.eop' succeeded. 0 Errors.
eve> Spawning process 'main'.
eve> Total time for code generation: 0.098000 ms.
eve> Code generation of main succeeded. 0 Errors.

Hello, World!

eve> Execution completed in 0.000030 seconds.
vm> Recieved shutdown signal. Waiting to deliver 0 messages...
vm> Shutdown complete.
```

Let's take a look at creating a simple class.

#### bean_counter.eop
```
class BeanCounter():
    count = 0

    def AddBeans(beans):
        count = count + beans
    end

    def GetCount():
        return count
    end
end

def main():
    counter = BeanCounter()

    for i in 0 to 100:
        counter->AddBeans(1)
    end

    beans = counter->GetCount()

    when beans:
        print("Collected " + to_string(beans) + " beans.")
    end
end
```

```
eople bean_counter.eop
```
```
Collected 100 beans.
```

This again probably looks familiar to you, except for one keyword: __when__. The result returned from `counter->GetCount()` was not a value, but instead a promise. That counter object you created was actually an extremely lightweight asynchronous process. All class instances in eople communicate asynchronously with each other. The "counter->GetCount()" expression is a message being passed to the counter object. Instead of returning the result of its computation immediately, it returns a promise of a future result being computed in the `counter` process.

Promises in Eople behave sort of like booleans. Except you don't evaluate them at a static moment in time, since their values are set to true in a separate process running in parallel to the current process. Instead of thinking about __if__ a promise result is ready, you need to start thinking temporally, in terms of __when__ the result is ready. In other languages, this type of pattern is usually implemented with callbacks.

You might wonder if creating a huge number of asynchronous processes and sending tons of async messages has a lot of overhead. Like Erlang however, Eople is optimized for this usage pattern. Let's try to spawn a million async objects and see how long it takes:

#### spawn_test.eop
```
def main():
    print("Spawning 1,000,000 processes.")
    start_time = get_time()
    for i in 0 to 1000000:
        process = SimpleClass()
    end

    delta = get_time() - start_time
    print("Total time spawning 1,000,000 processes: " + to_string(delta*1000.0) + " milliseconds." )
    print("Average process spawn time: " + to_string(delta) + " microseconds." )
end

class SimpleClass():
    counter = 0

    def Message(delta):
        counter = counter + delta
    end
end
```

On my aging AMD fx-8120, this is the result:
```
daniel@daniel-CM1831:~/git_repos/eople/examples$ eople spawn_test.eop
Spawning 1,000,000 processes.
Total time spawning 1,000,000 processes: 495.513 milliseconds.
Average process spawn time: 0.495513 microseconds.
```

0.5μs per process is not bad.

As for messaging overhead:
#### message_test.eop
```
def main():
    print("Messaging process 1,000,000 times.")
    process = SimpleClass()
    start_time = get_time()
    for i in 0 to 1000000:
        process->Message(1)
    end

    delta = get_time() - start_time
    print("Total time sending 1,000,000 messages: " + to_string(delta*1000.0) + " milliseconds." )
    print("Average message send time: " + to_string(delta) + " microseconds." )
end

class SimpleClass():
    counter = 0

    def Message(delta):
        counter = counter + delta
    end
end
```

Running this on the same linux machine:
```
daniel@daniel-CM1831:~/git_repos/eople/examples$ eople message_test.eop
Messaging process 1,000,000 times.
Total time sending 1,000,000 messages: 5353.45 milliseconds.
Average message send time: 5.35345 microseconds.
```

Again, only minor overhead for asynchronous messaging.

## Using the Eople Virtual Environment (E.V.E.)

In order to follow along, fire up the eople executable. This serves as the compiler, runtime, and can function as a REPL.

Typing help() will give you a look at some built in commands:
```
eople> help()
```

Let's get our feet wet with the REPL. How about typing in some expressions:
```
eople> 3 * 7 + 11
```

As you can see, E.V.E. will evaluate the expression and print the result.

Now, try assigning an expression to a variable:
```
eople> x = (11 + 13) * 17
```

Note that you usually don't have to specify the type of a variable. There are only a few cases where type inference won't help you, eg. when creating empty containers.

Trying to assign a value of a different type will give you an error:
```
eople> x = "a string"
```

Variables defined in the REPL can of course be referenced in future statements and expressions.
```
eople> print("Hello! x = " + x.to_string())
```

As you can probably guess, the ".to_string()" is necessary due to Eople's strong typing. Another point worth mentioning, is that x.to_string() is simply syntactic sugar for to_string(x).

Let's get some more basics out of the way.
```
eople> string_array = [ "Hello", "world.", "How", "are", "you?"]
```

Note that if you wanted to create an empty array of strings, you would have to explicitly specify the type as it would otherwise be impossible to infer the type at the time of variable declaration:
```
eople> empty_array = array(<string>)
```

Iterating over a container is straight forward. In this example, you'll need to use multiple lines. To do this in the REPL, you can hit ctrl-enter instead of enter at the end of the line. Alternatively, use the Scratchpad tab to write your code, then paste it into the terminal.
```
eople> for element in string_array:
......     print(element)
...... end
```

Conditionals are also unsurprising:
```
eople> if string_array.size() == 5:
......     string_array.pop()
......     print("New array size after pop: " + to_string(string_array.size()))
...... else:
......     print("Never going to get here")
...... end
```

Defining a function is again, unsurprising (don't worry, you're only about 2 minutes away from getting to the interesting stuff):
```
eople> def join(strings):
......     result = ""
......
......     for string in strings:
......         result = result + string + " "
......     end
......
......     return result
...... end
```

Now, to call the function, you could do it the old fashioned way. Or, you could use member function syntax like we did with to_string():
```
eople> joined_string = string_array.join()
eople> print(joined_string)
```

Ok, things are just about to get interesting. Let's go ahead and define a simple class:
```
eople> class BeanCounter():
......     count = 0
......
......     def AddBeans(beans):
......         count = count + beans
......     end
......
......     def GetCount():
......         return count
......     end
...... end
```

Constructing an object is straight forward:
```
eople> counter = BeanCounter()
```

How about we add some beans:
```
eople> for i in 0 to 100:
......     counter->AddBeans(1)
...... end
```

Then get the result:
```
eople> beans = counter->GetCount()
```

Finally, we get to what makes Eople interesting. That result you just got? You didn't necessarily get it yet. You got a promise of a result. That counter object you created was actually more like an asynchronous process - it's an object that you can asynchronously communicate with. The "counter->GetCount()" expression is actually an asynchronous message being passed to the counter object. Instead of returning the result of its computation, it returns a promise of a future result being computed in parallel.

Promises in Eople behave sort of like booleans. Except, you don't evaluate them at a static moment in time, since their values are set to true in a separate process running in parallel to the current process. Instead of thinking about IF a promise result is ready, you need to start thinking temporally, in terms of WHEN the result is ready:
```
eople> when beans:
......     print(beans.get_value())
...... end
```

You can think of this as the GetCount() function tunneling through a wormhole from a parallel time-line to bring us our result. Only within the protective confines of our temporal "when" bubble can the effects of the computational result be seen. (In order to get at the value, you have to explicitly extract it using the get_value() function. Future versions of the compiler will likely remove the need for an explicit call to get_value(), and automatically extract the result within a guarded "when" block.)

You can think of "when" statements as the temporal version of an "if" statement. Again, instead of the branch being taken IF it evaluates to true, the branch is taken WHEN it evaluates to true.

Given that "when" is the temporal version of a branch, how about a temporal version of a while loop? First, let's create a timer variable:
```
eople> later_comes = after(1)
```

The call to after(x) just returns a promise that becomes true after x milliseconds. The promise itself has no value to be extracted.

Let's also set up a local boolean to control our loop:
```
eople> loop_is_active = true
```

A temporal loop in Eople is defined using the "whenever" statement. The block is entered whenever variables in the condition are updated, and evaluate to true:

```
eople> whenever later_comes and loop_is_active:
......     print("Is it groundhogs day?")
......     // this causes the loop to re-trigger itself after 1 second
......     later_comes = after(1000)
...... end
```

To stop the loop, just set the loop control to false:
```
eople> loop_is_active = false
```

The whenever statement allows you to handle concurrent events in a more reactive way. For example, say you have an object that caches certain internal events, and flushes them to file after every n events when the buffer is full. Instead of bloating your event code, you can create a whenever loop that autonomously reacts to the filled buffer:

```
class Foo():
    event_log = array(<string>)

    whenever event_log.size() % 3 == 0:
        print("Flushing to file...")
        // ...
        event_log.clear()
    end

    def do_stuff():
        // do some stuff...
        // ...
        // log an event
        event_log.push("Hit some event.")
    end

    def do_other_stuff():
        // do some other stuff...
        // ...
        // log an event
        event_log.push("Hit some other event.")
    end
end

foo = Foo()
foo->do_stuff()
foo->do_other_stuff()
foo->do_stuff()
```

Keep in mind that Eople is a very young project (started at the end of 2013), and is in heavy development. You will see many things missing that you would expect to be there, and you will run into bugs. If you want to help Eople see its full potential, now is a great time to join in and contribute your ideas and code.

At this point, you should be feeling brave enough to take a look at the example code included in the distribution. To run the examples, you must first import them into the virtual environment:
```
import(../examples/uhmfufu.eop)
// View imported functions. In the future, externally visible functions must be within a namespace.
//   Until that feature is implemented, there is global visibility.
imported()
// For the moment, calling imported functions must use special syntax. This requirement will go away soon.
call(Uhmfufu::main)
```
