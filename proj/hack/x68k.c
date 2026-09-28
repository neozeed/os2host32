/*
	The x68000 aparently doesn't have signals or kill
	Obviously there should be a better way to capture
		- control c
		- control whatever else

But for now these are stubs to let it compile and barely run
*/
void signal()
{}
void kill(int i)
{}

