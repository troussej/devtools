##INIT

# fonts https://github.com/adobe-fonts/source-code-pro
# https://github.com/seebi/dircolors-solarized.git
#https://github.com/karlin/mintty-colors-solarized.git
# https://git-scm.com/book/en/v1/Git-Basics-Tips-and-Tricks

source $DEVTOOLS/scripts/git-completion.bash

# get current git branch name
function git_branch {
    export gitbranch=[$(git rev-parse --abbrev-ref HEAD 2>/dev/null)]
    if [ "$?" -ne 0 ]
      then gitbranch=
    fi
    if [[ "${gitbranch}" == "[]" ]]
      then gitbranch=
    fi
}

function hg_branch {
	export hgbranch=[$(hg branch 2>/dev/null)]
	if [ "$?" -ne 0 ]
      then hgbranch=
    fi
    if [[ "${hgbranch}" == "[]" ]]
      then hgbranch=
    fi
}
 
# set usercolor based on whether we are running with Admin privs
function user_color {
    id | grep "Admin" > /dev/null
    RETVAL=$?
    if [[ $RETVAL == 0 ]]; then
        usercolor="[0;35m";
    else
        usercolor="[0;32m";
    fi
}
 

 
# Set prompt and window title
inputcolor='[0;37m'
cwdcolor='[0;34m'
gitcolor='[1;31m'
user_color

# set TTYNAME
function ttyname() { export TTYNAME=$@; }
 
# Setup for window title
export TTYNAME=$$
function settitle() {
  p=$(pwd);
  let l=${#p}-80
  if [ "$l" -gt "0" ]; then
    p=..${p:${l}}
  fi
  t="$p"
  echo -ne "\e]2;$t\a\e]1;$t\a";
}
 
PROMPT_COMMAND='settitle; git_branch; hg_branch; history -a;'
export PS1='\[\e${usercolor}\][\u]\[\e${gitcolor}\]${gitbranch}\[\e${gitcolor}\]${hgbranch}\[\e${cwdcolor}\][$PWD]\[\e${inputcolor}\]\n$ '

#PROXY
proxyfile="$DEVTOOLS"/proxy/jtro.proxy.sh
if [ -f $proxyfile ]
	then
	source $proxyfile
fi


### UTILS
eval `dircolors ~/.dir_colors`
alias ls="ls --color"

alias ll='ls -lha'
alias la='ls -ah'

alias srcbrc='source ~/.bashrc'

alias grep='grep --color=auto'

#open explorer in current path
function explorer(){
	/bin/cygstart --explore "${1:-.}"
}


function locate(){
	find / -name $1
}		

function attr(){
	 grep -oP ${1}'="\K[^"]*' $2;
}

function cdata(){
	 grep -oP  $1'"><!\[CDATA\[\K[^\]]*' $2
}

function nodevalue(){
	 grep -oP  $1'>\K[^\<]*' $2
}


#usage chunk "Product Name" productId
function chunk(){
	echo $1"-"$2 | sed -e 's/ +/-/g' | awk '{print tolower($0)}'
}


function mkfileP() { mkdir -p "$(dirname "$1")" || return; touch "$1"; }

function findInFile(){
	grep -oP "(?<=$1=)(.*)$" server.log | sort -u
}

mkcdir ()
{
    mkdir -p -- "$1" && cd -P -- "$1"
}

grepless(){
	grep --color=always -r "$@" . | less -R
}
alias gl=grepless

st(){
	logpath=$1
	if [  $ISCYGWIN ]
	then
		logpath=`cygpath -w $logpath`
	fi
	sublime3 $logpath
}

copyst(){

	cp -f "$SUBLIME_HOME"/Packages/User/JTRO/ATG/* "$DEVTOOLS_HOME/st"
	cp -f "$SUBLIME_HOME"/Packages/User/*.sublime-settings "$DEVTOOLS_HOME/st_settings"
	cp -f "$SUBLIME_HOME"/Packages/User/*.sublime-keymap "$DEVTOOLS_HOME/st_settings"

}

loadst(){

	cp -f "$DEVTOOLS_HOME/st" "$SUBLIME_HOME"/Packages/User/JTRO/ATG/* 
	cp -f "$DEVTOOLS_HOME/st_settings/*.sublime-settings" "$SUBLIME_HOME"/Packages/User/ 
	cp -f "$DEVTOOLS_HOME/st_settings/*.sublime-keymap" "$SUBLIME_HOME"/Packages/User/

}

alias editbashrc='st ~/.bashrc'

setproxy(){
	cp -f $DEVTOOLS_HOME/proxy/jtro.proxy.sh.$1 $DEVTOOLS_HOME/proxy/jtro.proxy.sh
	srcbrc;
	if [  $ISCYGWIN ]
	then
		scriptPath=`cygpath -w $DEVTOOLS/proxy/win/setproxy.$1.bat`
		echo $scriptPath
		cmd /c $scriptPath
	fi
}

#HISTORIC

# if [[ $- == *i* ]]
# then
#     bind '"\e[A": history-search-backward'
#     bind '"\e[B": history-search-forward'
# fi

### COLOR MAN PAGES

export LESS_TERMCAP_mb=$(printf '\e[01;31m') # enter blinking mode – red
export LESS_TERMCAP_md=$(printf '\e[01;35m') # enter double-bright mode – bold, magenta
export LESS_TERMCAP_me=$(printf '\e[0m') # turn off all appearance modes (mb, md, so, us)
export LESS_TERMCAP_se=$(printf '\e[0m') # leave standout mode
export LESS_TERMCAP_so=$(printf '\e[01;33m') # enter standout mode – yellow
export LESS_TERMCAP_ue=$(printf '\e[0m') # leave underline mode
export LESS_TERMCAP_us=$(printf '\e[04;36m') # enter underline mode – cyan

#### GENERIC ATG

alias cdloc='cd $DYNAMO_HOME/localconfig'

#prefixes all the ids of an order
#usage order infFile outFile prefix
order(){
	$DEVTOOLS_HOME/scripts/order/convertOrder.sh $1 $2 $3
}

#set the log level in .properties in the localconfig
#usage setlog Level component path
#level must be capitalized (ex:  Debug, not debug)
#ex : setLog Info /some/Component
setlog(){
	level=$1
	value=$2
	file=$DYNAMO_HOME/localconfig$3.properties

	if [ ! -f $file ]; then
		mkfileP $file
	fi

	#remove all reference to logdebug
	sed -ine "/logging${level}=false/d" $file
	sed -ine "/logging${level}=true/d" $file

	echo "logging${level}=${value}" >> $file

	less -FX $file
}

colorAlternate(){
	#!/bin/bash
	read line
	#assume first line is the longest
	length=${#line}
	printf "\e[1;31m%-${length}s\e[0m\n" "$line"
	while read line
	do
	  printf "%-${length}s\n" "$line"
	  read line
	  printf "\e[7m%-${length}s\e[27m\n" "$line"
	done
	echo -e "\e[0m"
}

#returns all the local configurations that have logging enabled
checklog(){
	fetchLogData  | column -s "," -t | colorAlternate
}

fetchLogData(){
 prefix=$DYNAMO_HOME/localconfig
 suffix=".properties"
 prefixLength=${#prefix}
 suffixLength=${#suffix}

 propertiesFiles=$(find $LOCALCONFIG/ -type f -name "*.properties" )

 debugFiles=$( echo $propertiesFiles | xargs grep -l "^loggingDebug=true" )
 declare -A logDebug;
 for file in $debugFiles; do
 	logDebug[$file]=true;
 done

 infoFiles=$(echo $propertiesFiles |xargs grep -l "^loggingInfo=true")
 declare -A logInfo;
 for file in $infoFiles; do
 	logInfo[$file]=true;
 done

 traceFiles=$(echo $propertiesFiles |xargs grep -l "^loggingTrace=true")
 declare -A logTrace;
 for file in $traceFiles; do
 	logTrace[$file]=true;
 done

 warningFiles=$(echo $propertiesFiles|xargs grep -l "^loggingWarning=true")
 declare -A logWarning;
 for file in $warningFiles; do
 	logWarning[$file]=true;
 done

 errorFiles=$(echo $propertiesFiles |xargs grep -l "^loggingError=true")
 declare -A logError;
 for file in $errorFiles; do
 	logError[$file]=true;
 done


 allFiles="$debugFiles $traceFiles $infoFiles $warningFiles $errorFiles" 
 allFiles=$(echo $allFiles |tr ' ' '\n' |sort -u )
 echo "Component,Trace,Debug,Info,Warning,Error"
 for file in $allFiles; do
 	compName=${file:$prefixLength}
 	fileLength=${#compName}-$suffixLength
 	compName=${compName:0:$fileLength}
 	echo ${compName},${logTrace[$file]},${logDebug[$file]},${logInfo[$file]},${logWarning[$file]},${logError[$file]}
 done

}

#open the localconfig .properties of a component. Create the files if it doesn't exist
#ex: editlocalconfig /some/Component
editlocalconfig(){
	file=$DYNAMO_HOME/localconfig$1.properties

	if [ ! -f $file ]; then
		mkfileP $file
	fi
	st $file;

}

alias elc=editlocalconfig



repoItem(){
	$DEVTOOLS_HOME/scripts/repository/genRepoItem.sh $1
}

# grep all non ascii chars
nonascii(){
	grep -nP "[^\x00-\x7F]" $1
}

#BDA

bda_files="bda.user.js bda.css"

copybda(){
	for file in $bda_files; do
		cp $BDA_GM/$file $BDA_PROJECT/
	done
}


loadbda(){
	for file in $bda_files; do
		cp $BDA_PROJECT/$file $BDA_GM/
	done
}

bdaversion(){
	version=$1
	sed -i "s/@version .*$/@version ${version}/" $BDA_PROJECT/bda.user.js
	sed -i "s/css?version=.*$/css?version=${version}/" $BDA_PROJECT/bda.user.js
}

atgTail(){
	tail -f $1 |$DEVTOOLS/scripts/simpleColorizer.sh
}
alias at=atgTail

# export INV_HOME=$SEPH_BATCH/INV
# export INV_IN=$INV_HOME/atgin
# export INV_REF=$INV_HOME/INV_REF

# inv(){
# 	fileName=INV`printf "%010d" $1`;
# 	filePath=$INV_IN/$fileName
# 	echo "copying INV ref file to $filePath"
# 	cp  $INV_REF $filePath;
# }

########### ST related

grepless(){
	grep --color=always $1 $2 |less -R
}
alias gl=grepless

testSpeed(){
	time for i in {1..10} ; do bash -c "echo Hello" ; done
}

timeDiff(){
	date -d @$(( $(date -d "$2" +%s) - $(date -d "$1" +%s) )) -u +'%H:%M:%S'
}

datestamp(){
	date +'%Y%m%d'
}

