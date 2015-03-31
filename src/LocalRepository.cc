//********************************************************
/**
 * @file  LocalRepository.cc 
 *
 * @brief Local webfiles repository
 *
 * @author T.Descombes (descombes@lpsc.in2p3.fr)
 *
 * @version 1	
 * @date 27/01/14
 */
//********************************************************


#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <fstream>
#include <streambuf>
#include <sstream>
#include "navajo/LogRecorder.hh"
#include "navajo/LocalRepository.hh"


/**********************************************************************/

bool LocalRepository::loadFilename_dir (const string& alias, const string& path, const string& subpath="")
{
    struct dirent *entry;
    int ret = 1;
    DIR *dir;
    string fullPath=path+subpath;


    dir = opendir (fullPath.c_str());
    if (dir == NULL) return false;
    while ((entry = readdir (dir)) != NULL ) 
    {
	    if (!strcmp(entry->d_name,".") || !strcmp(entry->d_name,"..")) continue;

	    if ((entry->d_type == DT_REG || entry->d_type == DT_LNK) && entry->d_name)
	      filenamesSet.insert(alias+subpath+"/"+entry->d_name);

	    if (entry->d_type == DT_DIR)
	      loadFilename_dir(alias, path, subpath+"/"+entry->d_name);
    }
    return true;
}

/**********************************************************************/

void LocalRepository::addDirectory( const string& alias, const string& dirPath)
{
  char resolved_path[4096];

  string newalias=alias;
  while (newalias.size() && newalias[0]=='/') newalias.erase(0, 1);
  while (newalias.size() && newalias[newalias.size()-1]=='/') newalias.erase(newalias.size() - 1);
  
  if (realpath(dirPath.c_str(), resolved_path) == NULL)
    return ;

  if (!loadFilename_dir(newalias, resolved_path))
	  return ;

  aliasesSet.insert( pair<string, string>(newalias, resolved_path) );

}

/**********************************************************************/

void LocalRepository::clearAliases()
{
  filenamesSet.clear();
  aliasesSet.clear();
}

/**********************************************************************/

bool LocalRepository::fileExist(const string& url)
{
  return filenamesSet.find(url) != filenamesSet.end();
}

/**********************************************************************/

void LocalRepository::printFilenames()
{
  for (std::set<std::string>::iterator it = filenamesSet.begin(); it != filenamesSet.end(); it++)
    printf ("%s\n", it->c_str() );
}

/**********************************************************************/

bool LocalRepository::getFile(HttpRequest* request, HttpResponse *response)
{
  bool found=false;
  const string *alias, *path;
  string url = request->getUrl();
  size_t webpageLen;
  unsigned char *webpage;
  pthread_mutex_lock( &_mutex );

  if (!fileExist(url)) { pthread_mutex_unlock( &_mutex); return false; };

  for (std::set< pair<string,string> >::iterator it = aliasesSet.begin(); it != aliasesSet.end() && !found; it++)
  {
    alias=&(it->first);
    if (!(url.compare(0, alias->size(), *alias)))
    {
      path=&(it->second);
      found= true;
    }
  }
    
  pthread_mutex_unlock( &_mutex);
  if (!found) return false;
   
  string resultat, filename=url;

  filename.replace(0, alias->size(), path->c_str());

  FILE *pFile = fopen ( filename.c_str() , "rb" );
  if (pFile==NULL)
  { return false; }

  // obtain file size.
  fseek (pFile , 0 , SEEK_END);
  webpageLen = ftell (pFile);
  rewind (pFile);

  if ( (webpage = (unsigned char *)malloc( webpageLen+1 * sizeof(char))) == NULL )
    return false;
  size_t nb=fread (webpage,1,webpageLen,pFile);
  if (nb != webpageLen)
  {
    char logBuffer[150];
    snprintf(logBuffer, 150, "Webserver : Error accessing files '%s'", filename.c_str() );
    LOG->append(_ERROR_, logBuffer);
    return false;
  }
  
  fclose (pFile);
  response->setContent (webpage, webpageLen);
  return true;
}



