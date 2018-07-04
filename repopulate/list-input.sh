# split by chunks of n files
ninput=100

for file in $(ls $HOME/aafu/repopulate/*.list)
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
			echo "transfer_input_files=\$ENV(HOME)/aafu/repopulate/$f.dir/input.$index,\$ENV(HOME)/aafu/repopulate/copy-files.sh"
			echo "arguments = input.$index"
			echo "queue"
			(( index +=1 ))
			n=0
		fi
	done
done

if (( $n > 0 && $n < $ninput ))
then
	echo "transfer_input_files=\$ENV(HOME)/aafu/repopulate/$f.dir/input.$index,\$ENV(HOME)/aafu/repopulate/copy-files.sh"
	echo "arguments = input.$index"
	#echo "queue"
fi