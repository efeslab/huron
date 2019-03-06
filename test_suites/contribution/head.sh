exit_if_file_not_found()
{
  DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" > /dev/null 2>&1 && pwd )"
  if [ ! -f $DIR/$1 ]; then
    echo "File: $1 Not found"
    exit 1
  fi
}
