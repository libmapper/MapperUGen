Mapper : UGen {
    *enable {
        play {
            MapperEnabler.kr;
            FreeSelf.kr(Impulse.kr(1));
        }
    }

    *disable {
        play {
            MapperDisabler.kr;
            FreeSelf.kr(Impulse.kr(1));
        }
    }

    *makeInSignalBus {
        arg server, name, min, max;
        var bus = Bus.control(server);
        {Out.kr(bus.index, MapIn.kr(name, min, max))}.play;
        ^bus;
    }
}

MapIn : UGen {
    *kr { arg name, min = 0, max = 1;
        var ascii = name.ascii;
        ^this.new1('control', *[min, max, ascii.size].addAll(ascii));
    }
}

MapOut : UGen {
    *kr { arg in, name, min = 0, max = 1;
        var ascii = name.ascii;
        this.new1('control', *[in, min, max, ascii.size].addAll(ascii));
        ^0.0;
    }
}

MapperEnabler : UGen {
    *kr {
        this.new1('control');
        ^0.0;
    }
}

MapperDisabler : UGen {
    *kr {
        this.new1('control');
        ^0.0;
    }
}
