10 rem *** open comms
11 open 15,16,15, ""
12 open 2,16,2,"https://api.adviceslip.com/advice"

20 rem *** receive, parse, print data
21 print#15, "jsonparse,2"
22 print#15, "jq,2,/slip/advice"
23 print#15, "biteparse,2,80"
24 input#2, v$: print v$;
25 if(st and 64)=0 then goto 24

30 rem *** close
31 close 2: close 15