string div(float a, float b) {
    if (a == 0.0 || b == 0.0) {
	return sprintf("% 8s", "-");
    }

    return sprintf("% 8.1f ", (1-a/b)*100);
}

float average(array(float|int) a) {
    return `+(@a)/(float)sizeof(a);
}

array combine(mixed ... a) {
    function f = a[-1];
    a = a[..<1];
    array ret = allocate(sizeof(a[0]));
    for (int i = 0; i < sizeof(a[0]); i++) {
	ret[i] = f(@(a[*][i]));
    }

    return ret;
}

float standard_deviation(array(float|int) a) {
    float av = average(a);
    a = map(a, `-, av);
#ifdef COMBINE
    return sqrt(`+(@combine(a, a, `*)));
#else
    return sqrt(`+(@map(a, pow, 2))/(float)sizeof(a));
#endif
}

class Value(float|int v, void|float deviation) {
    int samples;

    float mean_deviation() {
	if (floatp(deviation) && deviation != 0.0) {
	    return (samples > 1) ? deviation / sqrt(samples) : deviation;
	}

	return 0.0;
    }

    int|float round_err(float err) {
	int power = -(int)ceil(Math.log10(err))+1;
	err = ceil(err * pow(10.0, power))*pow(10.0, -power);
	return (err > 1.0) ? (int)err : err;
    }

    string _sprintf(int type) {
	float mdev = mean_deviation();

	if (type == 'f') {
	    return sprintf("%f\t%f", (float)v, floatp(mdev) ? mdev : 0.0);
	}

	if (floatp(deviation) && deviation != 0.0) {
	    // this = 456.3455 +/- 0.456 -> 456.3(0.5)
	    // this = 2345678.324 +/- 345.4 -> 2345700(400)
	    // mdev = 0.004345
	    // err = 5 = ceil(4.345)
	    // power = 4 
	    int power = -(int)ceil(Math.log10(mdev))+1;
#if 1
	    return sprintf("%."+(string)max(0, power)+"f(%O) +/- %O", (float)v, round_err(mdev), round_err(deviation));
#else
	    return sprintf("(%.4f ~ %f)", (float)v, mdev);
#endif
	}
	return (string)v;
    }

    mixed cast(string type) {
	switch (type) {
	case "int":
	    return (int)v;
	case "float":
	    return (float)v;
	default:
	    error("cannot cast %O to %O\n", this, type);
	}
    }

#define SCALE_OP(_) this_program ` ## _(mixed o) {		\
	this_program x = this_program(v _ o, deviation _ o);	\
	x->samples = samples;				    	\
	return x;					    	\
    }								\

    SCALE_OP(/)
    SCALE_OP(*)

#define ADD_OP(_)  this_program ` ## _(mixed o) {					\
	this_program n;									\
	if (floatp(o) || intp(o)) {							\
	    n = this_program(v _ o, deviation);						\
	} else if (objectp(o) && Program.inherits(object_program(o), this_program)) {	\
	    n = this_program(v _ o->v, deviation + o->deviation);			\
	}										\
	n->samples = n->deviation > 0.0							\
	    ? (int)pow(n->deviation / (mean_deviation() + o->mean_deviation()), 2)	\
	    : 0;									\
	return n;									\
    }

    ADD_OP(+)
    ADD_OP(-)
}											

class Result {
    Value mem, time;
    string e;

    void create(int|Value mem, float|Value time, void|string e, void|string name) {
	this_program::mem = objectp(mem) ? mem : Value(mem);
	this_program::time = objectp(time) ? time : Value(time);
	if (!objectp(mem)) this_program::mem->samples = -3;
	if (!objectp(time)) this_program::time->samples = -3;
	this_program::e = e;
	this_program::name = name;
    }

    string name;

    string _sprintf(int type) {
	return sprintf("%-30s%-23s%-27s", name, (mem/1024)->_sprintf('O')+" kb", (time*1000)->_sprintf('O') + " ms");
    }

#define PROXY_OP(_)	this_program ` ##_(mixed o) {					\
	if (objectp(o) && Program.inherits(object_program(o), this_program)) {		\
	    return this_program(mem _ o->mem, time _ o->time, e, name);			\
	}										\
	return this_program(mem _ o, time _ o, e, name);				\
    }
    PROXY_OP(-)
}

Value combine_values(array(Value) a) {
    Value n;
    if (sizeof(a) == 0) error("foo\n");
    float deviation = standard_deviation(a->v);
    float av = average(a->v);
    // calculate average and average standard deviation
    n = Value(av, deviation);
    n->samples = sizeof(a);
    return n;
}


mapping(int:array(Value)) parse_raw(string s) {
    mapping(int:mixed) m = ([]), mb = ([]);
    array(string) a = s/"\n";
    if (a[-1] == "") a = a[..sizeof(a)-2];
    
    for (int i = 0; i < sizeof(a); i++) {
	array(string) tmp = a[i]/"\t";
	int n = (int)tmp[0];
	float t = (float)tmp[1];
	int bytes = (int)tmp[2];
	if (!m[n]) {
	    m[n] = ({ });
	    mb[n] = ({ });
	}
	m[n] += ({ Value(t) });
	mb[n] += ({ Value(bytes) });
    }

    foreach (m; int n; array t) {
	m[n] = ({ combine_values(t), combine_values(mb[n]) });
    }

    return m;
}

int main(int argc, array(string) argv) {
    mapping(int:array(Value)) m;


    m = parse_raw(Stdio.read_file(argv[1]));

    foreach (sort(indices(m));; int n) {
	array a = m[n];
	write("%d\t%f\t%f\n", n, a[0], a[1]);
    }

    return 0;
}
