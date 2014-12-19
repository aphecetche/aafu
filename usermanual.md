# SAF2 User Manual

## Introduction

SAF2 is simply an evolution of SAF1, with more workers (108 instead of 48) and more disk space (30 TB instead of 12). The master name has changed from `nansafmaster` to `nansafmaster2` simply, so the connection is now done with :

	TProof::Open("user@nansafmaster2.in2p3.fr",option);
	
where `option` can be :

- "" (empty, the default) to get all available workers
- "workers=1x" to get one proof worker per machine (that would yield 11 workers in SAF2)
- "workers=8x" to get 8 workers per machine (try not to go over this)
- "masteronly" to skip the connection to the proof workers and start a conversation only with the master (it's faster to start, and is more than enough for everything related to staging, for instance)
	
Note that the username should be specified in the Open command. It might work without but we observed that sometimes the connection just hangs when the username is not specified, while it's 
always OK with the username...

Please remember that you must have a valid alien token before attempting the connection and that you must have the following line in your `.rootrc`:

	XSec.GSI.DelegProxy: 2
	
If you do not want to touch your `.rootrc` you can also do :

	root[] gEnv->SetValue("XSec.GSI.DelegProxy", "2");
	
before connecting (each time).

Compared to SAF1 though, the big news in SAF2 lies in how the datasets are dealt with.

SAF2 now uses what is called dynamic datasets (SAF1 was using static ones). This has consequences on how datasets are staged and used.

A dynamic dataset is a query (string) to the alien catalog, e.g.

    Find;FileName=AliAOD.Muons.root;BasePath=/alice/data/2011/LHC11h/000169838/ESDs/pass2_muon/AOD119
    
 To speed up things the result of the alien query is cached, so not all queries actually go to the alien catalog. The query string is used both to request staging of the dataset (see below) but also in all the subsequent usage of the dataset, i.e. during analysis. So, where you were doing, in SAF1 :
 
    mgr->StartAnalysis("proof","/MUON/laphecetc/LHC11h_pass2_muon_AODMUON119_000169838")
    
 you should now do, in SAF2 :
 
    mgr->StartAnalysis("proof","Find;FileName=AliAOD.Muons.root;BasePath=/alice/data/2011/LHC11h/000169838/ESDs/pass2_muon/AOD119")
    
 While it's quite a bit longer to type for the end user, the dynamic datasets solve a number of issues we had with static ones. Basically all the interaction with the stager daemon was getting longer the more datasets there were (SAF1 had about 5000 datasets) , making the whole system impossible to scale. Worse, the stager daemon was continuously scanning _all_ the datasets... With dynamic datasets, the interaction with the stager daemon is limited to the datasets that are _being_ staged. Once staging is done no further interaction is needed with the stager daemon.
 
## Available data on SAF2

Discovering what data is already present on SAF2 is a bit less easy than on SAF1 (where you could do `gProof->ShowDataSets("/*/*/*")` 

Instead, a script is in place to generate, at a given frequency (once per day), [HTML reports](https://nansafmaster2.in2p3.fr "saf disk reports") of the data sitting on SAF2 disks.

When the data you want to use is not available, you'll have to request a staging for it. You can do it all by yourself (see next sections) or ask for help (send a mail to alice-saf at subatech dot in2p3 dot fr).

## Staging datasets
 
### Requesting the staging
 
 The actual staging request can be performed in two ways (basic and libmyaf), see the next two sections below. In both cases, please assess first the total size of the data you request. Then use common sense as to which size is reasonable or not, baring in mind that the total available storage space on SAF2 is about 29 TB. 
 
 Warning : the dynamic datasets feature is a relatively recent one, so it will only work if you've selected a recent enough Root version, e.g. 
 
     > root
 	 TProof::Mgr("user@nansafmaster2.in2p3.fr")->SetROOTVersion("VO_ALICE@ROOT::v5-34-18");
	 .q
	
As a reminder, you can list the Root versions with :

    > root
 	TProof::Mgr("user@nansafmaster2.in2p3.fr")->ShowROOTVersions();
 	.q

Mind that all interactions with `TProof::Mgr` must be done outside any Proof session. Please also remember to change the Root version back to the one corresponding to the AliRoot tag you are using (see valid combinations at [http://alimonitor.cern.ch/packages/]) before starting your analysis...

#### Basic method

This method is the most direct one, but also probably the most cumbersome one. Nevertheless it's good to know and will certainly help if the following method (AF class) does not work for some reason...

First thing is to construct a query string for the data you want to stage. The query string closely ressembles what you would use directly from alien to search for files :

	const char* query = "Find;FileName=AliAOD.Muons.root;BasePath=/alice/data/2011/LHC11h/000169838/ESDs/pass2_muon/AOD119";
   
Then, before requesting an actual staging, it's always a good idea to see if your query returns the list of files you expected :

 	gProof->ShowDataSet(query);

which should bring you (immediately or after some delay, depending on the cache status) a list of the files corresponding to your query :

<table>
   <tr><td>#. SC</td><td>Entries</td><td> Size</td><td> URL</td></tr>
   <tr><td>sc</td><td>n.a.<td> 73.1 MiB</td><td>root://nansafmaster2.in2p3.fr//alice/data/2011/LHC11h/000169838/ESDs/pass2_muon/AOD119/0001/AliAOD.Muons.root</td></tr>
   <tr><td></td><td><td></td><td>alien:///alice/data/2011/LHC11h/000169838/ESDs/pass2_muon/AOD119/0001/AliAOD.Muons.root</td></tr>
   <tr><td>sc</td><td>n.a.<td> 73.1 MiB</td><td>root://nansafmaster2.in2p3.fr//alice/data/2011/LHC11h/000169838/ESDs/pass2_muon/AOD119/0002/AliAOD.Muons.root</td></tr>
   <tr><td></td><td><td></td><td>alien:///alice/data/2011/LHC11h/000169838/ESDs/pass2_muon/AOD119/0002/AliAOD.Muons.root</td></tr>
   <tr><td>sc</td><td>n.a.<td> 73.1 MiB</td><td>root://nansafmaster2.in2p3.fr//alice/data/2011/LHC11h/000169838/ESDs/pass2_muon/AOD119/0003/AliAOD.Muons.root</td></tr>
   <tr><td></td><td><td></td><td>alien:///alice/data/2011/LHC11h/000169838/ESDs/pass2_muon/AOD119/0003/AliAOD.Muons.root</td></tr>
   <tr><td colspan=4>etc...</td></tr>
</table>  
 
together with a summary :

	There are 32 file(s) in dataset: 32 (100.0%) matched your criteria (SsCc)
	Total size    : 1.2 GiB
	Staged (S)    :   0.0 %
	Corrupted (C) :   0.0 %
	Default tree  : (no default tree)
</code>

In the table above the first column will indicate the status of the file :

- s (lower case S letter) means the file is *not* staged
- S (upper case S letter) means the file *is* staged
- c (lower case C letter) means the file is *not* corrupted (and/or not staged)
- C (upper case C letter) means the file *is* corrupted

The second column gives the number of entries in the tree (if available, i.e. if the file is not staged it cannot be opened to read the tree so that information can not be accessible), the third gives the file size (available in any case as this information is part of the catalog), and the fourth one gives the URLs of the file. The last URL is always the alien path : it is the "raw" result of the query, so to speak. The first URL is the actual path on the SAF. If it's starting with the master URL then it means the file is *not* staged (yet). The hostname of a staged file would be that of one of the servers (nansafXXX)

The query string must start with `Find;`. It must include the two keywords `FileName` (file to look for) and `BasePath` (where to start the search in the alien catalog). Mind the fact that keywords are case sensitive. In addition the query string can optionally include more keywords :

- `Tree` : the name of the tree in the file
- `Anchor` : the name of the anchor in case the FileName is Root archive 

Once you're happy with the list of files returned by your query, you can actually ask for the staging :

	gProof->RequestStagingDataSet(query);
	
You can then monitor the progress using : 

	gProof->ShowStagingStatusDataSet(query,opt);

where `opt` can be "" (by default) or `"C"` to show files marked as corrupted, or `"Sc"` to show files that have been staged and are not corrupted, for instance.

Note that with a recent enough Root version (>= 5.34/18), the query string can also contain the keyword `ForceUpdate` (case sensitive, as the other keywords) to force the query to actually go to the catalog (and refresh the status of the local files) whatever the cache status is. 

#### The libmyaf method

Using the `myaf` library can make the staging somewhat easier, in particular if you are dealing with run lists. Note there's nothing in that library that you cannot do using the basic commands above. It's just supposed to be more user friendly, by constructing the query string for you and looping over a run list.

##### installing libmyaf

The code is kept in GitHub. You can obtain it using `git clone https://github.com/aphecetche/aafu.git`. 
In the code directory, just do `make`. 
This will create both a `libmyaf.so` library and a `myaf` executable.

##### using libmyaf

Start root and load the `libmyaf.so`library (note that if you start `root` from the code directory, the `rootlogon.C` found there will be used and it is already loading the `libmyaf.so` library for you).

Then create an object of class VAF :

  VAF* af = VAF::Create("saf2");
  
the parameter is the name of the analysis facility you want to deal with. It will only work if you have in your `$HOME` directory a `.aaf` file which define a number of variables for the relevant analysis facility. For instance : 

	saf2.user: laphecet
  	saf2.master: nansafmaster2.in2p3.fr
  	saf2.dynamic: true
  	saf2.home: /home/PROOF-AAF
  	saf2.log: /var/log
  	saf2.datadisk: /data

Then the key method of the `VAF` class is CreateDataSets(whichRuns, dataType,esdPass,aodPassNumber,basename) where :

- whichRuns is either a single runNumber (integer) or the name of a text file containing one run number per line
- dataType (case insensitive) tells which type of data is to be staged. The following are recognized currently :
	-  *stat* -> [EventStat_temp.root]
  	- *zip* -> [root_archive.zip] or [aod_archive.zip]
  	- *esd* -> [AliESDs.root]
    - *esd_outer* -> [AliESDs_Outer.root]
   	- *esd_barrel* -> [AliESDs_Barrel.root]
  	- *aod* -> [AliAOD.root]
  	- *aodmuon* -> [AliAOD.Muons.root]
  	- *aoddimuon* -> [AliAOD.Dimuons.root]
  	- *esd.tag* -> [ESD.tag]
  	- *raw* -> [raw data file]
 - esdPass is the reconstruction pass (e.g. muon_pass2)
 - aodPassNumber is the filtering pass number :
		- < 0 to use AODs produced during the reconstruction stage (1 per original ESDs)
		- = 0 to use the merged version of the AODs produced during the reconstruction stage (1 per several original ESDs
		- > 0 to indicate a filtering pass
- basename is the alien path of where to start looking for the data, generally of the form /alice/data/year/period/ESDs/... The more specific you can be with that basename, the faster the alien catalog query will be. That basename is sometimes a bit tricky to get right, so please see the examples below.

Note that by default the AF class is in "dry" mode, so the CreateDataSets method will _not_ actually create a dataset but show you what would be staged if we were not in dry mode. Once happy with the returned list of files, you can go for the real thing. 

For instance, to create a dataset with all the muon AODs for run 197341 (period LHC13f) you would do :

	// see if we get the expected list of files
	root [1] vaf->CreateDataSets(197341,"aodmuon","muon_pass2",0,"/alice/data/2013/LHC13f");
	Starting master: opening connection ...
	Starting master: OK                                                 
	PROOF set to sequential mode
	Find;BasePath=/alice/data/2013/LHC13f/000197341/ESDs/muon_pass2/AOD/	*;FileName=AliAOD.Muons.root;Tree=/aodTree
	TFileCollection  -  contains: 38 files with a size of 145870354 bytes, 0.0 % staged - default tree name: '/aodTree'
	TFileCollection  -  contains: 38 files with a size of 145870354 bytes, 0.0 % staged - default tree name: '/aodTree'
	The collection contains the following files:
	Collection name='THashList', class='THashList', size=38
 	root://nansafmaster2.in2p3.fr//alice/data/2013/LHC13f/000197341/ESDs/muon_pass2/AOD/001/AliAOD.Muons.root -|-|- feeb35f2b70df1de8535c93398efaf33
 	root://nansafmaster2.in2p3.fr//alice/data/2013/LHC13f/000197341/ESDs/muon_pass2/AOD/002/AliAOD.Muons.root -|-|- f1c14dd02a07164fc8e25af8778550a5
 	root://nansafmaster2.in2p3.fr//alice/data/2013/LHC13f/000197341/ESDs/muon_pass2/AOD/003/AliAOD.Muons.root -|-|- 86715f3f3716b41c16eb6089b3cb5207
 	root://nansafmaster2.in2p3.fr//alice/data/2013/LHC13f/000197341/ESDs/muon_pass2/AOD/004/
 		etc...
 		nruns=1 nfiles = 38 | size =    0.14 GB
 		
 		
Note that in the first lines you get the query that is used internally, as in the basic method above

You might see a bunch of those messages : "Srv err: No servers have the file" : they are harmless.

Once the list of file is ok, you switch to "do-something-for-real" mode :

		vaf->DryRun(false);
		// do it for real
		vaf->CreateDataSets(197341,"aodmuon","muon_pass2",0,"/alice/data/2013/LHC13f");
	
And you should see a line indicating the _request_ has been issued :
	
		Mst-0 | Info in <TXProofServ::HandleDataSets>: Staging request registered for Find;BasePath=/alice/data/2013/LHC13f/000197341/ESDs/muon_pass2/AOD/*;FileName=AliAOD.Muons.root;Tree=/aodTree

### Monitoring staging progress

Directly from root you can, given a query string, ask what is the status of that query :

	gProof->ShowStagingStatusDataSet("Find;BasePath=/alice/data/2013/LHC13f/000197341/ESDs/muon_pass2/AOD/*;FileName=AliAOD.Muons.root;Tree=/aodTree")

This information is only available during the staging and a little while after it has been completed. Once the staging is completed you can use gProof->ShowDataSet() to e.g. list corrupted files, if any.

Another way to see if something is actually happening is to look at the stager daemon log using the myaf program.

### myaf program

The little utility program `myaf` will help you perform some operations on an AF.  Like libmyaf there's nothing magic in it, it's just for convenience.

	> myaf 
	Usage : myaf aaf command [detail]
	- aaf is the nickname of the AAF you want to connect to (must be including in the $HOME/.aaf configuration file)
	- command is what you want myaf to do : 
	-- reset [hard] : reset the AF
	-- ds pattern : show the list of dataset(s) matching the pattern
	-- showds : show the content of one dataset (or several if option is the name of a text file containing the IDs of datasets)
	-- clear : clear the list of packages from the user
	-- packages : show the list of available packages 

	-- stagerlog : (advanced) show the logfile of the stager daemon
	-- df : (advanced) get the disk usage on the AF
	-- xferlog filename : (advanced) get the log of a failed transfer 
	-- conf : (advanced) show the configuration files of the AF 

### Lost in translation

For your convenience, please find below some translation examples from the dataset names in SAF1 vs the query strings in SAF2.

<table class="dslist">
<tr><td>/default/lmassacr/SIM_Jpsi_coherent_LHC13f6a_AOD_</td>
<td> Find;BasePath=/alice/sim/2013/LHC13f6a/%6d/AOD/;FileName=AliAOD.root</td></tr>
<tr><td>/MUON/laphecet/LHC11h_pass2_muon_AODMUON119_</td>
<td>Find;BasePath=/alice/data/2011/LHC11h/%09d/ESDs/pass2_muon/AOD119;FileName=AliAOD.Muons.root</td></tr>
<tr><td>/MUON/laphecet/LHC13b2_efix_p1_AOD000_000195593</td>
<td>Find;BasePath=/alice/sim/2013/LHC13b2_efix_p1/195593/AOD/;FileName=AliAOD.root</td></tr>
<tr><td>/MUON/laphecet/SIM_DPMJET_LHC13b2_efix_p1_000195644</td><td>Find;BasePath=/alice/sim/2013/LHC13b2_efix_p1/195644/AOD/;FileName=AliAOD.root</td></tr>
<tr><td>/MUON/laphecet/LHC13f4_AOD000_000196601</td><td>Find;BasePath=/alice/sim/2013/LHC13f4/196601/AOD/;FileName=AliAOD.root</td></tr>
<tr><td>/MUON/laphecet/LHC13f4_AOD000_000197348</td><td>Find;BasePath=/alice/sim/2013/LHC13f4/197348/AOD/;FileName=AliAOD.root</td></tr>
<tr><td>/MUON/laphecet/SIM_JPSI_LHC13d_CynthiaTuneWithRejectList_</td><td>Find;BasePath=/alice/cern.ch/user/l/laphecet/Analysis/LHC13d/simjpsi/CynthiaTuneWithRejectList/%6d;FileName=AliAOD.Muons.root </td></tr>
<tr><td>/MUON/laphecet/SIM_JPSI_LHC13e_CynthiaTuneWithRejectList_</td><td> Find;BasePath=/alice/cern.ch/user/l/laphecet/Analysis/LHC13e/simjpsi/CynthiaTuneWithRejectList/%6d;FileName=AliAOD.Muons.root </td></tr>
<tr><td>/MUON/laphecet/SIM_JPSI_LHC13f_CynthiaTuneWithRejectList_</td><td> Find;BasePath=/alice/cern.ch/user/l/laphecet/Analysis/LHC13f/simjpsi/CynthiaTuneWithRejectList/%6d;FileName=AliAOD.Muons.root </td></tr>
<tr><td>/MUON/laphecet/LHC13d_muon_pass2_AOD134_ </td><td>Find;BasePath=/alice/data/2013/LHC13d/%09d/ESDs/muon_pass2/AOD134;FileName=AliAOD.root </td></tr>
<tr><td>/MUON/laphecet/LHC13e_muon_pass2_AOD134_ </td><td>Find;BasePath=/alice/data/2013/LHC13e/%09d/ESDs/muon_pass2/AOD134;FileName=AliAOD.root </td></tr>
<tr><td>/MUON/laphecet/LHC13f_muon_pass2_AOD000_ </td><td>Find;BasePath=/alice/data/2013/LHC13f/%09d/ESDs/muon_pass2/AOD/;FileName=AliAOD.root </td></tr>
<tr><td>/MUON/laphecet/SIM_PSIPRIME_MLEONCIN_</td><td> Find;BasePath=/alice/cern.ch/user/m/mleoncin/Modified_Shapes_PsiP_Production/Ap/out/%6d;FileName=AliAOD.root </td></tr>
<tr><td>/default/jmartinb/SIM_JPSI_Indra_LHC12h_pass2_muon_calo_iter3_</td><td> Find;BasePath=/alice/cern.ch/user/j/jmartinb/SIM/LHC12hi/pass2_muon_calo_iter3/%6d/;FileName=AliAOD.Muons.root </td></tr><tr><td>/default/jmartinb/SIM_JPSI_Indra_LHC12i_pass2_muon_calo_iter3_ </td><td>Find;BasePath=/alice/cern.ch/user/j/jmartinb/SIM/LHC12hi/pass2_muon_calo_iter3/%6d/;FileName=AliAOD.Muons.root </td></tr>
<tr><td>/MUON/laphecet/LHC14g1_AOD000_ </td><td>Find;BasePath=/alice/sim/2014/LHC14g1/%6d/AOD/;FileName=AliAOD.root</td></tr>
<tr><td>/MUON/laphecet/LHC14g1a_AOD000_ </td><td>Find;BasePath=/alice/sim/2014/LHC14g1a/%6d/AOD/;FileName=AliAOD.root</td></tr>
<tr><td>/MUON/laphecet/LHC13c_pass2_AOD139_FILTER_AODMUONWITHTRACKLETS_LHC13c_000195644 </td><td>Find;BasePath=/alice/data/2013/LHC13c/000195644/ESDs/pass2/AOD139;FileName=AliAOD.root</td></tr>
<tr><td>/MUON/laphecet/LHC12d_pass1_AOD_000184968_FILTER_AODMUONWITHTRACKLETS </td><td>Find;BasePath=/alice/data/2012/LHC12d/000184968/pass1/AOD/;FileName=AliAOD.root</td></tr>
<tr><td>SIM_EMBEDDING_LHC11h_pass2_refit_mergedAOD_ </td><td>Find;BasePath=/alice/cern.ch/user/p/ppillot/Sim/LHC11h/embedPass2/RefitAODs/merged/%6d/;FileName=AliAOD.Muons.root</td></tr>
<tr><td>SIM_GammaGamma_LHC13f6d_AOD_ </td><td>Find;BasePath=/alice/sim/2013/LHC13f6d/%6d/AOD/;FileName=AliAOD.Muons.root</td></tr>
<tr><td>SIM_Jpsi_incoherent_LHC13f6b_AOD_ </td><td>Find;BasePath=/alice/sim/2013/LHC13f6b/%6d/AOD/;FileName=AliAOD.Muons.root</td></tr>
<tr><td>SIM_PsiPrime_LHC13f6c_AOD_</td><td> Find;BasePath=/alice/sim/2013/LHC13f6c/%6d/AOD/;FileName=AliAOD.Muons.root</td></tr>
<tr><td>sim_pure_jpsi_LHC11h_ </td><td>Find;BasePath=/alice/cern.ch/user/l/lmassacr/pure_jpsi_stat/output/%6d/;FileName=AliAOD.Muons.root</td></tr>
<tr><td>data_LHC13g_AOD155_ </td><td>Find;BasePath=/alice/data/2013/LHC13g/%09d/pass1/AOD155;FileName=AliAOD.Muons.root </td></tr>
</table>



