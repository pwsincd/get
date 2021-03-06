#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <iostream>

#include "constants.h"

#include "Get.hpp"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/istreamwrapper.h"

using namespace std;
using namespace rapidjson;

Get::Get(const char* config_dir, const char* defaultRepo)
{
	this->defaultRepo = defaultRepo;
	
	// the path for the get metadata folder
	string config_path = config_dir;
	
	string* repo_file = new string(config_path + "repos.json");
	this->repos_path = repo_file->c_str();
	string* package_dir = new string(config_path + "packages/");
	this->pkg_path = package_dir->c_str();
	string* tmp_dir = new string(config_path + "tmp/");
	this->tmp_path = tmp_dir->c_str();
	
	
	//	  printf("--> Using \"./sdroot\" as local download root directory\n");
	//	  mkdir("./sdroot", 0700);
	
	mkdir(config_path.c_str(), 0700);
	mkdir(package_dir->c_str(), 0700);
	mkdir(tmp_dir->c_str(), 0700);
	
	cout << "--> Using \"" << *repo_file << "\" as repo list" << endl;
	
	// load repo info
	this->loadRepos();
	this->update();
}

int Get::install(Package* package)
{
	// found package in a remote server, fetch it
	bool located = package->downloadZip(this->tmp_path);
	
	if (!located)
	{
		// according to the repo list, the package zip file should've been here
		// but we got a 404 and couldn't find it
		cout << "--> Error retrieving remote file for [" << package->pkg_name << "] (check network or 404 error?)" << endl;
		return false;
	}
	
	// install the package, (extracts manifest, etc)
	package->install(this->pkg_path, this->tmp_path);
	
	cout << "--> Downloaded [" << package->pkg_name << "] to sdroot/" << endl;
	
	// update again post-install
	update();
	return true;
}

int Get::remove(Package* package)
{
	package->remove(this->pkg_path);
	cout << "--> Uninstalled [" << package->pkg_name << "] package" << endl;
	update();
	
	return true;
}

int Get::toggleRepo(Repo* repo)
{
	repo->enabled = !repo->enabled;
	update();
	return true;
}

/**
Load any repos from a config file into the repos vector.
**/
void Get::loadRepos()
{
	repos.clear();
	const char* config_path = repos_path;
	
	ifstream* ifs = new ifstream(config_path);
	
	if (!ifs->good() || ifs->peek() == std::ifstream::traits_type::eof())
	{
		cout << "--> Could not load repos from " << config_path << ", generating default repos.json" << endl;
		
		Repo* defaultRepo = new Repo("Default Repo", this->defaultRepo);
		
		Document d;
		d.Parse(generateRepoJson(1, defaultRepo).c_str());
		
		std::ofstream file(config_path);
		StringBuffer buffer;
		Writer<StringBuffer> writer(buffer);
		d.Accept(writer);
		file << buffer.GetString();
		
		ifs = new ifstream(config_path);
		
		if (!ifs->good())
		{
			cout << "--> Could not generate a new repos.json" << endl;
			
			// manually create a repo, no file access
			Repo* repo = new Repo();
			repo->name = "Switchbru";
			repo->url = this->defaultRepo;
			repo->enabled = true;
			repos.push_back(repo);
			return;
		}
		
	}
	
	IStreamWrapper isw(*ifs);
	
	Document doc;
	doc.ParseStream(isw);
	
	if (!doc.HasMember("repos"))
	{
		cout << "--> Invalid format in " << config_path << endl;
		return;
	}
	
	const Value& repos_doc = doc["repos"];
	
	// for every repo
	for(Value::ConstValueIterator it=repos_doc.Begin(); it != repos_doc.End(); it++)
	{
		Repo* repo = new Repo();
		repo->name = (*it)["name"].GetString();
		repo->url = (*it)["url"].GetString();
		repo->enabled = (*it)["enabled"].GetBool();
		repos.push_back(repo);
	}
	
	return;
}

void Get::update()
{
	// clear current packages
	packages.clear();
	
	// fetch recent package list from enabled repos
	for (int x=0; x<repos.size(); x++)
	{
		if (repos[x]->enabled)
				repos[x]->loadPackages(&packages);
	}
	
	// check for any installed packages to update their status
	for (int x=0; x<packages.size(); x++)
		packages[x]->updateStatus(this->pkg_path);
}

int Get::validateRepos()
{
	if (repos.size() == 0)
	{
		printf("There are no repos configured!\n");
		return ERR_NO_REPOS;
	}
	
	return 0;
}
