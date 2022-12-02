string parsedebug_opt()   { return ""; }
string parsedebug_usage() { return "parsedebug on|off\n"; }
string parsedebug_help()  { return "Turn on/off eppic parsing debug output\n"; }


int parsedebug()
{
    while(argc) {
        printf("argv[1]='%s'\n", argv[1]);
        if(argv[1]=="on") parsedebugon();
        else if(argv[1]=="off") pasedebugoff();
        else break;
        return 1;
    }
    printf("%s", parsedebug_usage());
    return 0;
}
