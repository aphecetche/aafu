# split by chunks of n files
transfer() {
    echo -n "transfer_input_files="
    echo -n "\$ENV(HOME)/aafu/repopulate/$1.dir/input.$2,"
    echo -n "\$ENV(HOME)/aafu/repopulate/copy-files.sh,"
    echo -n "\$ENV(HOME)/.PoD/user_worker_env.sh,"
	echo
    echo "arguments = input.$index $USER $server"
    echo "queue"
}

ninput=250
server=nansaf10

for file in $(ls $HOME/aafu/repopulate/$server.list)
do
    
    n=0
    index=0
    
    f=$(basename $file)
    
    mkdir -p $HOME/aafu/repopulate/$f.dir
    
    cd $HOME/aafu/repopulate/$f.dir
    rm -rf input.*
    
    for l in $(cat $file)
    do
        echo $l >> input.$index
        (( n += 1))
        if (( $n % $ninput == 0 ))
        then
            transfer $f $index $server
            (( index +=1 ))
            n=0
        fi
    done
done

if (( $n > 0 && $n < $ninput ))
then
    transfer $f $index $server
fi