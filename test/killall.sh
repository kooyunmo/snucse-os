ps -A | grep trial | cut -d' ' -f3 | xargs kill -9
ps -A | grep selector | cut -d' ' -f3 | xargs kill -9
ps -A | grep rotd | cut -d' ' -f3 | xargs kill -9
ps -A | grep trial | cut -d' ' -f2 | xargs kill -9
ps -A | grep selector | cut -d' ' -f2 | xargs kill -9
ps -A | grep rotd | cut -d' ' -f2 | xargs kill -9
