/**
 * Copyright (C) 2007-2008 Kelly Goetsch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *
 * ATGLogColorizer accepts piped input or log files from JBoss, WebLogic, WebSphere,
 * or DAS application servers and color-codes the output. While this works for generic
 * application server output, it is also able to recognize ATG-specific output.
 *
 * Version 1.0 released 4/27/07 by Kelly Goetsch
 * Version 1.1 released 5/03/07 by Kelly Goetsch
 *			-Found bug with startDynamoOnJBOSS.bat by where the output coming from runAssembler
 *			 wasn't being printed to the screen. This was due to the script writing a bunch of
 *			 null characters to the screen, which caused conflicts when calling printf(line.c_str());
 * Version 1.2 released 4/14/08 by Kelly Goetsch
 *			-Enhanced the ability of the tool to recognize different types of messages
 *			-Fixed bug that caused a crash when printing out certain % sequence
 *			-Added ability to recognize thread dumps
 */

#include <stdafx.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <fstream>
#include <signal.h>
#include <windows.h>

using namespace std;

// this holds all of the attributes of the window before the application is launched
// when exiting, the console is restored to these attributes
WORD originalwindowAttributes;
CONSOLE_SCREEN_BUFFER_INFO csbi;

HANDLE console; // object representing the console (eg. DOS window)

const char NULL_CHARACTER = '\0';

const string RELEASE_NUMBER="1.2"; // as in ATGLogColorizer vX.X

// how many previous lines of output are held in saved?
const int NUM_SAVED_LINES = 20;

/*
	here we keep three arrays: previous line types, previous trimmed lines, and previous lines.
	this is done so that we can look through the previous lines to determine what type they are,
	which helps determine the current line. for instance, some stack traces are followed by a
	). looking back on the past line types, we can determine this is part of an error and it is
	colored appropriately
*/
int previousLineTypes[NUM_SAVED_LINES];
string previousLines[NUM_SAVED_LINES];
string previousLinesTrimmed[NUM_SAVED_LINES];

// used in setTextColor. windows.h sets colors...not using ANSI escape sequences
const int INFO_COLOR_WINDOWS = 2; // green
const int WARNING_COLOR_WINDOWS = 6; // cyan
const int DEBUG_COLOR_WINDOWS = 0; // white
const int ERROR_COLOR_WINDOWS = 1; // red
const int INTRO_COLOR_WINDOWS = 0; // white
const int OTHER_COLOR_WINDOWS = 3; // yellow
const int EXITING_COLOR_WINDOWS = 5; // purple
const int NUCLEUS_COLOR_WINDOWS = 5; // purple

/*
	int determineLineType is the function responsible for determining a single output line's type.
	it returns one of the six constants
*/
const int INFO_LINE = 0;
const int WARNING_LINE = 1;
const int DEBUG_LINE = 2;
const int ERROR_LINE = 3;
const int OTHER_LINE = 4;
const int NUCLEUS_LINE = 5;

bool contains(string haystack, string needle)
{
	return (haystack.find(needle) != string::npos);
}

bool startsWith(string haystack, string needle)
{
	return contains(haystack.substr(0, needle.size()), needle);
}

bool endsWith(string a, string b)
{
	if (a.size() < b.size())
	{
		return false;
	}
	return contains(a.substr(a.size()-b.size(), a.size()), b);
}

/*
 *	scripts like startDynamoOnJBOSS.bat sometimes stick a bunch of null characters in the
 *	output. this method replaces all of those null characters with empty spaces
 */
string stripNullChars(string str)
{
	while (str.find_last_of(NULL_CHARACTER) != string::npos)
	{
		size_t start = str.find_last_of(NULL_CHARACTER);
		str = str.replace(start, 1, " ");
	}
	return str;
}

// performs left and right trim
string trim(const string& String)
{
	string::size_type string_size = String.size();
	string::size_type start = 0;
	for(; start != string_size; ++start)
	{
		if(!isspace(String[start]))
			break;
	}

	string::size_type end = string_size;
	for(; end != start; --end)
	{
		if(!isspace(String[end-1]))
		break;
	}

	return string(String.substr(start, end - start));
}

// sets text color of console. any text printed to the screen after a color has been set
// is colored as set. SetConsoleTextAttribute() is provided via windows.h
void setTextColor(int color, HANDLE console)
{
	switch (color)
	{
		case 0:
			SetConsoleTextAttribute(console,
			FOREGROUND_INTENSITY | FOREGROUND_RED |
			FOREGROUND_GREEN | FOREGROUND_BLUE);
			break;
		case 1:
			SetConsoleTextAttribute(console,
			FOREGROUND_INTENSITY | FOREGROUND_RED);
			break;
		case 2:
			SetConsoleTextAttribute(console,
			FOREGROUND_INTENSITY | FOREGROUND_GREEN);
			break;
		case 3:
			SetConsoleTextAttribute(console,
			FOREGROUND_INTENSITY | FOREGROUND_RED |
			FOREGROUND_GREEN);
			break;
		case 4:
			SetConsoleTextAttribute(console,
			FOREGROUND_INTENSITY | FOREGROUND_BLUE);
			break;
		case 5:
			SetConsoleTextAttribute(console,
			FOREGROUND_INTENSITY | FOREGROUND_RED |
			FOREGROUND_BLUE);
			break;
		case 6:
			SetConsoleTextAttribute(console,
			FOREGROUND_INTENSITY | FOREGROUND_GREEN |
			FOREGROUND_BLUE);
			break;
		case 7:
			SetConsoleTextAttribute(console,
			BACKGROUND_INTENSITY | BACKGROUND_INTENSITY);
			break;
	} // end switch
}

bool isSopErrorLine(string trimmedLine)
{
	if (
				contains(trimmedLine, "Ids cannot be null") ||
				contains(trimmedLine, "Ids cannot be empty") ||
				contains(trimmedLine, "Attempt to add a NULL item to the repository") ||
				contains(trimmedLine, "Attempt to add an item to the repository without specifying") ||
				contains(trimmedLine, "Invalid data type name:") ||
				contains(trimmedLine, "Invalid item class name") ||
				contains(trimmedLine, "Invalid item descriptor name") ||
				(contains(trimmedLine, "No property named") && contains(trimmedLine, "could be found in the item descriptor")) ||
				(contains(trimmedLine, "No item with ID") && contains(trimmedLine, "could be found in item descriptor")) ||
				contains(trimmedLine, "is not queryable and thus cannot be used in this query") ||
				contains(trimmedLine, "Error initializing id generator") ||
				contains(trimmedLine, "Error reading list or array index from the database") ||
				contains(trimmedLine, "Attempt to create a sub-property query expression for the property") ||
				contains(trimmedLine, "Attempt to create a query using transient property") ||
				contains(trimmedLine, "Attempt to create a case-insenstive query with no SQL") ||
				contains(trimmedLine, "does not appear to be defined correctly in the database") ||
				contains(trimmedLine, "Query or QueryExpression object that is null or was not created by this repository") ||
				contains(trimmedLine, "invalid array of Query objects") ||
				(contains(trimmedLine, "The argument") && contains(trimmedLine, "cannot be null")) ||
				contains(trimmedLine, "Multi-valued properties may not be used") ||
				contains(trimmedLine, "using QueryExpressions that cannot be compared") ||
				contains(trimmedLine, "No default properties are defained") ||
				(contains(trimmedLine, "The query operator") && contains(trimmedLine, "is invalid")) ||
				contains(trimmedLine, "Attempt to execute a query with pQueryOptions = null") ||
				contains(trimmedLine, "SQL Repository not configured with DatabaseTableInfos") ||
				contains(trimmedLine, "Could not remove entry or entries for item descriptor") ||
				contains(trimmedLine, "An SQL error was encountered") ||
				contains(trimmedLine, "An SQL error was encountered") ||
				contains(trimmedLine, "Unable to decode composite ID") ||
				contains(trimmedLine, "has incorrectly configured IdSpaces") ||
				contains(trimmedLine, "Id values must match Id column count") ||
				contains(trimmedLine, "Unable to set Id values of table") ||
				contains(trimmedLine, "Attempt to execute or build a text comparison query") ||
				(contains(trimmedLine, "Unable to convert ID") && contains(trimmedLine, "to type")) ||
				contains(trimmedLine, "Unable to convert composite ID element") ||
				contains(trimmedLine, "Unable to initialize stored procedure helper") ||
				(contains(trimmedLine, "Arguments were provided for the query") && contains(trimmedLine, "which does not contain parameters")) ||
				contains(trimmedLine, "Invalid parameter type passed to query") ||
				contains(trimmedLine, "Unable to rebuild this expression.") ||
				contains(trimmedLine, "No arguments supplied for the parameter query") ||
				contains(trimmedLine, "Wrong number of arguments supplied for parameter query") ||
				contains(trimmedLine, "Null return property specified for query") ||
				contains(trimmedLine, "is not readable, and cannot be specified ") ||
				contains(trimmedLine, "is not a GSA property, and cannot be specified") ||
				contains(trimmedLine, "is transient, and cannot be a return property") ||
				contains(trimmedLine, "is multi-valued, and cannot be a return property") ||
				contains(trimmedLine, "Null dependent property specified") ||
				contains(trimmedLine, "Null or blank sql string argument entered for DirectSqlQuery") ||
				contains(trimmedLine, "Unable to create a DirectSqlQuery against a transient item descriptor") ||
				(contains(trimmedLine, "Unable to load class") && contains(trimmedLine, "for input parameter at index")) ||
				contains(trimmedLine, "Invalid parameter type at index") ||
				contains(trimmedLine, "Error initializing sql query") ||
				contains(trimmedLine, "Error parsing template") ||
				contains(trimmedLine, "No template files defined, be sure the property") ||
				contains(trimmedLine, "Unable to read template file") ||
				contains(trimmedLine, "No XML parser could be found") ||
				contains(trimmedLine, "Unable to find the id space") ||
				contains(trimmedLine, "Invalid protocol magic number read") ||
				contains(trimmedLine, "Exception while reading events from data input stream") ||
				contains(trimmedLine, "No current transaction for getPropertyValue()") ||
				contains(trimmedLine, "Error setting the RQL filter string") ||
				contains(trimmedLine, "Unable to load database meta data for columns in table") ||
				contains(trimmedLine, "Attempt to perform a Sybase full text search query on property") ||
				contains(trimmedLine, "Attempt to perform a DB2 full text search query on property") ||
				contains(trimmedLine, "Attempt to set value of property") ||
				contains(trimmedLine, "An error occurred processing an invalidate cache entry") ||

				contains(trimmedLine, "*** failed to clone super-type") ||
				contains(trimmedLine, "can't read properties") ||
				contains(trimmedLine, "unkown bean:") ||
				contains(trimmedLine, "can't introspect property:") ||
				contains(trimmedLine, "unkown property:") ||
				contains(trimmedLine, "can't set property:") ||
				contains(trimmedLine, "Naming Exception caught") ||
				contains(trimmedLine, "Error: caught exception") ||

				contains(trimmedLine, "no getter for:") ||
				contains(trimmedLine, "NumberFormatException reading schema info cache") ||
				contains(trimmedLine, "does not exist in a table space accessible by the data source") ||
				contains(trimmedLine, "Found a one-to-many shared table definition in versioned case with one side using") ||
				contains(trimmedLine, "Found shared table definition in versioned case with only one asset version column") ||

				// from /atg/deployment/common/Resources.properties
				contains(trimmedLine, "Error parsing file") ||
				contains(trimmedLine, "deployment topology failed to load properly") ||
				contains(trimmedLine, "no JNDI name defined for JNDI transport of agent") ||
				contains(trimmedLine, "no transport found for JNDI name") ||
				contains(trimmedLine, "error looking up transport") ||
				contains(trimmedLine, "no targets defined in topology definition file") ||
				contains(trimmedLine, "no transport defined for agent") ||
				contains(trimmedLine, "no transport type defined for agent") ||
				contains(trimmedLine, "unknown transport type") ||
				contains(trimmedLine, "no URI defined for RMI transport of agent") ||
				contains(trimmedLine, "could not instantiate RMI server-side agent transport with URI") ||
				contains(trimmedLine, "to an indeterminate snapshot due to an interruption in the committed apply phase") ||
				contains(trimmedLine, "Simulating failure : DeploymentAgent.debugApplyFailIndex is set to") ||
				contains(trimmedLine, "Manifest application aborted at server request") ||
				contains(trimmedLine, "An error was encountered applying manifest data before any data was committed") ||
				contains(trimmedLine, "Data store switch preparation aborted at server request") ||
				contains(trimmedLine, "is either not configured or failed to start up properly") ||
				contains(trimmedLine, "The version manager is either not configured or failed to start up properly") ||
				contains(trimmedLine, "The manifest manager is either not configured or failed to start up properly") ||
				contains(trimmedLine, "The transaction manager is either not configured or failed to start up properly") ||
				contains(trimmedLine, "The rmi server is either not configured or failed to start up properly") ||
				contains(trimmedLine, "The topology manager is either not configured or failed to start up properly") ||
				contains(trimmedLine, "The deployment server failed to start-up properly") ||
				contains(trimmedLine, "received unknown deployment command") ||
				contains(trimmedLine, "forcing initialization of snapshot from") ||
				contains(trimmedLine, "cannot initialize snapshot, the agent is either active or already has an snapshot") ||
				contains(trimmedLine, "because the agent is inaccessible") ||
				contains(trimmedLine, "agent is locked by a different deployment") ||
				contains(trimmedLine, "deployment server system version does not match this agent") ||
				contains(trimmedLine, "attempt to resume or rollback a deployment on this agent but the agent has been changed by another deployment") ||
				contains(trimmedLine, "only full deployments are allowed when the agent snapshot is uninitialized") ||
				contains(trimmedLine, "was not found on the local agent") ||
				(contains(trimmedLine, "cannot enter phase") && contains(trimmedLine, "from phase")) ||
				contains(trimmedLine, "could not get manifest stream for writing manifest") ||
				contains(trimmedLine, "manifest stream is null for install of manifest") ||
				contains(trimmedLine, "error closing manifest stream : stream is being ignored") ||
				contains(trimmedLine, "A maintained Status object could not be cloned to create a safe copy to return") ||
				contains(trimmedLine, "A maintained Status could not write to file") ||
				contains(trimmedLine, "could not delete Status file") ||
				contains(trimmedLine, "error encountered reading in persisted status") ||
				contains(trimmedLine, "cannot interrupt a deployment in state") ||
				contains(trimmedLine, "A system error was encountered trying to lookup the RMI URI") ||
				contains(trimmedLine, "transport error from agent") ||
				contains(trimmedLine, "transport failed to start or is otherwise uninitialized") ||
				contains(trimmedLine, "could not send manifest to agent") ||
				contains(trimmedLine, "error reading manifest stream") ||
				contains(trimmedLine, "transport error installing manifest on agent") ||
				contains(trimmedLine, "error closing manifest stream") ||
				contains(trimmedLine, "deployment server is starting up with an uninitialized topology") ||
				contains(trimmedLine, "no topology XML configured") ||
				contains(trimmedLine, "could not remove completed deployment") ||
				contains(trimmedLine, "topology cannot reinit due to deployment") ||
				contains(trimmedLine, "error closing agent transport for target") ||
				contains(trimmedLine, "recovered deployment status is either not from this server") ||
				contains(trimmedLine, "there is a target with no name : each target must be named") ||
				contains(trimmedLine, "no agents were given deployment responsibilities in target") ||
				contains(trimmedLine, "no agents defined in target") ||
				contains(trimmedLine, "there is an agent with no name in target") ||
				contains(trimmedLine, "due to error from transport") ||
				contains(trimmedLine, "suggesting an unclean shutdown") ||
				contains(trimmedLine, "hard reset requested from user") ||
				contains(trimmedLine, "mis-match in live data store name of switchable data stores") ||
				contains(trimmedLine, "error encountered reverting deployment switch") ||
				contains(trimmedLine, "error encountered preparing switchable for switch") ||
				contains(trimmedLine, "all files not deleted from") ||
				contains(trimmedLine, "cannot initialize snapshot on target") ||
				contains(trimmedLine, "forcing initialization of snapshot on target") ||
				contains(trimmedLine, "due to error from agent") ||
				contains(trimmedLine, "could not discern snapshot due to error from target") ||
				contains(trimmedLine, "error mismatch in snapshot on target") ||
				contains(trimmedLine, "has a current deployment that cannot be removed") ||
				contains(trimmedLine, "error recovering deployment from status") ||
				contains(trimmedLine, "cannot be instantiated because the deployment target") ||
				contains(trimmedLine, "An unidentified deployment cannot be instantiated due to errors") ||
				contains(trimmedLine, "cannot be instantiated due to errors accessing the repository") ||
				contains(trimmedLine, "An error occurred attempting to move deployment") ||
				contains(trimmedLine, "a repository level error occurred during deployment initialization") ||
				contains(trimmedLine, "An error occurred attempting to delete deployment") ||
				contains(trimmedLine, "A transaction-level error occurred while trying to delete deployment") ||
				contains(trimmedLine, "encountered a transaction-level error while preparing Target") ||
				contains(trimmedLine, "cannot be started because there is already a current deployment") ||
				contains(trimmedLine, "could not be made the current deployment in order to start it") ||
				contains(trimmedLine, "the deployment is flagged as a revert but has more than one project") ||
				contains(trimmedLine, "the target has no initial snapshot") ||
				contains(trimmedLine, "should have a Snapshot by now but does not") ||
				contains(trimmedLine, "encountered a versioning error building the manifest") ||
				contains(trimmedLine, "encountered a system level deployment error during data transfer") ||
				contains(trimmedLine, "No destination repositories or virtual file systems were configured for this deployment") ||
				contains(trimmedLine, "could not be resolved as a Nucleus component") ||
				contains(trimmedLine, "The source virtual file system could not be found for the following file asset") ||
				contains(trimmedLine, "encountered an error with manifest") ||
				contains(trimmedLine, "A call to the current deployment running remotely on another deployment server") ||
				contains(trimmedLine, "is no longer the current deployment and thus could not be called") ||
				contains(trimmedLine, "An RMI error encountered calling remote current deployment") ||
				contains(trimmedLine, "suggesting an unclean shutdown") ||
				contains(trimmedLine, "but could find no such manifest") ||
				contains(trimmedLine, "cannot be started, either it was previously started or the deployment queue is running and this deployment is not next in the queue.") ||
				contains(trimmedLine, "cannot stop a deployment that is in a non-active or non-error state") ||
				contains(trimmedLine, "since the deployment has not stopped due to an error") ||
				contains(trimmedLine, "the deployment has started and must either complete successfully or be stopped in order to be deleted") ||
				contains(trimmedLine, "unrecognized deployment type") ||
				contains(trimmedLine, "cannot be found in the VersionManager : full deployment is required") ||
				contains(trimmedLine, "cannot perform an online deployment on target") ||
				contains(trimmedLine, "cannot perform an incremental deployment on target") ||
				contains(trimmedLine, "error communicating with target:agent") ||
				contains(trimmedLine, "could not lock target:agent") ||
				contains(trimmedLine, "error preparing target:agent") ||
				contains(trimmedLine, "error loading manifest on target:agent") ||
				contains(trimmedLine, "error installing manifest on target:agent") ||
				contains(trimmedLine, "error applying manifest on target:agent") ||
				contains(trimmedLine, "error activating deployment on target:agent") ||
				contains(trimmedLine, "event interrupt on target:agent") ||
				contains(trimmedLine, "error from target:agent") ||
				contains(trimmedLine, "Unexpected error occured. See log for details.") ||
				contains(trimmedLine, "do not have the same live data store : ") ||
				contains(trimmedLine, "Cannot deploy to target") ||
				contains(trimmedLine, "does not match current target snapshot : ") ||
				contains(trimmedLine, "unexpected state returned telling target:agent") ||
				contains(trimmedLine, "transport error unlocking target:agent") ||
				contains(trimmedLine, "error stopping deployment on target:agent") ||
				contains(trimmedLine, "agent errors encountered while stopping deployment") ||
				contains(trimmedLine, "error deleting manifest") ||
				contains(trimmedLine, "agent errors encountered while deleting manifests") ||
				contains(trimmedLine, "Deployment manifests could not be deleted from the agent") ||
				contains(trimmedLine, "encountered an exception while loading") ||
				contains(trimmedLine, "An exception was encountered while installing Manifest") ||
				contains(trimmedLine, "An exception was encountered switching data stores") ||
				contains(trimmedLine, "An exception was encountered sending update events to affected VirtualFileSystems") ||
				contains(trimmedLine, "runtime exception caught from event listener") ||
				contains(trimmedLine, "Failed to connect to agent ") ||
				contains(trimmedLine, "This agent not allowed to be absent for a deployment") ||
				contains(trimmedLine, "error resolving CMS catalog for deployment checks") ||
				contains(trimmedLine, "error updating foreign repository references") ||
				contains(trimmedLine, "Running deployment cannot be changed") ||
				contains(trimmedLine, "error resetting shadow") ||
				contains(trimmedLine, "Target is already initialized with a snapshot") ||
				contains(trimmedLine, "has pending or current deployment. It cannot be deleted or updated") ||
				contains(trimmedLine, "The name was given as a branch from which to initialize the new target branch") ||
				contains(trimmedLine, "When creating a new target the source target to initialize from is required") ||
				contains(trimmedLine, "cannot be deleted.  It is choosen to act as an initialization source") ||
				contains(trimmedLine, "Target preparation failed because the one-time server-side target initialization encountered an error") ||
				contains(trimmedLine, "due to lower level errors") ||
				contains(trimmedLine, "could not be found in the version manager for rollback") ||
				contains(trimmedLine, "because the Project is not checked in and does not have locked assets") ||
				contains(trimmedLine, "as a new Project because the Project has already been deployed to the target") ||
				contains(trimmedLine, "A system level error ") ||
				contains(trimmedLine, "could not be found in the Publishing repository.") ||
				contains(trimmedLine, "A transaction-level error occurring while trying to create a") ||
				contains(trimmedLine, "cannot be back-deployed to Project") ||
				contains(trimmedLine, "cannot revert a null Project") ||
				contains(trimmedLine, "cannot revert Project ID") ||
				contains(trimmedLine, "Exception encountered while trying to revert Project") ||
				contains(trimmedLine, "A transaction-level error occurring while trying to revert Project") ||
				contains(trimmedLine, "but there is no merge workspace associated with the project") ||
				contains(trimmedLine, "is marked as completed but the workspace, for the workspace name associated with it") ||
				contains(trimmedLine, "cannot be started : Target site") ||
				contains(trimmedLine, "cannot be reverted from deployment target site") ||
				contains(trimmedLine, "A transaction-level error occurring while trying to initialize Target site") ||
				contains(trimmedLine, "could not find snapshot") ||
				contains(trimmedLine, "internal error: unexpected diff from version manager") ||
				contains(trimmedLine, "must have exactly two underlying data sources to be used for deployment") ||
				contains(trimmedLine, "not a GSARepository. Instead it is of type:") ||
				contains(trimmedLine, "error creating shadow for:") ||
				contains(trimmedLine, "not a VirtualFileSystem. Instead it is of type:") ||
				contains(trimmedLine, "cannot create temp file:") ||
				contains(trimmedLine, "no manifest manager at") ||
				contains(trimmedLine, "no transaction manager at") ||
				contains(trimmedLine, "no version manager at") ||
				contains(trimmedLine, "no repository registry a") ||
				contains(trimmedLine, "no repository at ") ||
				contains(trimmedLine, "invalid starting index:") ||
				contains(trimmedLine, "invalid ending index:") ||
				contains(trimmedLine, "batch size must be either -1 or a postive integer") ||
				contains(trimmedLine, "unrecognized argument: ") ||
				contains(trimmedLine, "you must specify a data file") ||
				contains(trimmedLine, "you must specifiy at least one repository or -all for exports") ||
				contains(trimmedLine, "does not appear to be valid data file") ||
				contains(trimmedLine, "internal error reserving the id for the repository item") ||
				contains(trimmedLine, "attempt to export the versioned repository") ||
				contains(trimmedLine, "I/O error creating deferred update store") ||
				contains(trimmedLine, "I/O error writing int value") ||
				contains(trimmedLine, "could not find repository service") ||
				contains(trimmedLine, "could not find item descriptor") ||
				contains(trimmedLine, "could not find virtual file system") ||
				contains(trimmedLine, "no snapshot diff returned for") ||
				contains(trimmedLine, "internal error: unrecognized deployment type:") ||
				contains(trimmedLine, "A deployment cannot be created without a project.") ||
				contains(trimmedLine, "Cannot revert project") ||
				contains(trimmedLine, "An error occurred while importing topology") ||
				contains(trimmedLine, "Error occurred while invalidating the destination repository caches.") ||
				contains(trimmedLine, "No target repository mapping defined for") ||
				(contains(trimmedLine, "state change") && contains(trimmedLine, "received event interrupted from")) ||
				(contains(trimmedLine, "data file") && contains(trimmedLine, "does not exist")) ||
				(contains(trimmedLine, "invalid value") && contains(trimmedLine, "for argument:")) ||
				(contains(trimmedLine, "The deploy time of Deployment") && contains(trimmedLine, "could not be changed to")) ||
				(contains(trimmedLine, "for target") && contains(trimmedLine, "cannot be started twice")) ||
				(contains(trimmedLine, "Snapshot") && contains(trimmedLine, "could not be retrieved for Project")) ||
				(contains(trimmedLine, "Project with ID") && contains(trimmedLine, "is required to deploy Project(s)")) ||
				(contains(trimmedLine, "requested destination") && contains(trimmedLine, "not found")) ||
				(contains(trimmedLine, "data source for repository:") && contains(trimmedLine, "is not a switching data source")) ||
				(contains(trimmedLine, "data file") && contains(trimmedLine, "is not readable")) ||
				(contains(trimmedLine, "data file") && contains(trimmedLine, "is not writable")) ||
				contains(trimmedLine, "The connection pool failed to initialize propertly") ||
				(contains(trimmedLine, "The suppplied DataSource JNDI name") && contains(trimmedLine, "did not resolve to a DataSource")) ||
				contains(trimmedLine, "No Transaction could be found or created for the current thread") ||
				contains(trimmedLine, "failed to obtain the current Transaction from the TransactionManager") ||
				contains(trimmedLine, "transaction demarcation should be controled through JTA interfaces") ||
				contains(trimmedLine, "the currentDataSource property is NULL") ||
				contains(trimmedLine, "the dataSources property is NULL or contains no data sources") ||
				contains(trimmedLine, "is not recognized as the name of one of the data sources configured for this SwitchingDataSource") ||
				contains(trimmedLine, "mis-match between Transaction and Connection : FakeXA forces") ||
				contains(trimmedLine, "attempting to use a closed connection") ||
				contains(trimmedLine, "error reclaiming resource") ||
				contains(trimmedLine, "Synchronization detected probable missing Connection.close()") ||

				// /atg/adapter/gsa/xml/ParserResources.properties
				contains(trimmedLine, " has parsing errors.") ||
				contains(trimmedLine, "Fatal error parsing file") ||
				contains(trimmedLine, "Warning parsing file ") ||
				contains(trimmedLine, "File contains duplicate definition of item-descriptor ") ||
				contains(trimmedLine, "You must supply an item-descriptor attribute for the print-item tag") ||
				contains(trimmedLine, "You supplied an invalid item-descriptor") ||
				contains(trimmedLine, "should not have both super-type and copy-from attributes") ||
				contains(trimmedLine, "has an invalid item-descriptor for the super-type attribute") ||
				contains(trimmedLine, "has an invalid item-descriptor for the copy-from attribute") ||
				contains(trimmedLine, "must specify a valid property name for the sub-type-property attribute") ||
				contains(trimmedLine, "must specify a property for the sub-type-property") ||
				contains(trimmedLine, "must specify a valid property for the display-property attribute") ||
				contains(trimmedLine, "must specify a valid property for the version-property attribute") ||
				contains(trimmedLine, "must specify valid properties for the text-search-properties attribute") ||
				contains(trimmedLine, "must specify a valid integer for the cache-size attribute") ||
				contains(trimmedLine, "must specify a valid integer for the cache-timeout attribute") ||
				contains(trimmedLine, "must have a table tag with type=") ||
				contains(trimmedLine, "cannot have the sub-type-property attribute on it") ||
				contains(trimmedLine, "but is missing at least one of content-property, folder-id-property, or one of content-name-property") ||
				contains(trimmedLine, "but is missing at least one of folder-id-property, or one of content-name-property, content-path-property") ||
				contains(trimmedLine, "has a version-property which is not a number type.") ||
				(contains(trimmedLine, "Your attribute ") && contains(trimmedLine, "refers to a non-existent property")) ||
				contains(trimmedLine, "refers to a property that is not a repository property descriptor") ||
				contains(trimmedLine, "must have type attribute of primary, auxiliary, or multi.  You have") ||
				contains(trimmedLine, "which is not a sub-class of GSAPropertyDescriptor.") ||
				contains(trimmedLine, "has a property whose data-type is not valid for a multi table:") ||
				contains(trimmedLine, "is missing an item-descriptor.") ||
				contains(trimmedLine, "is missing an id-column-name attribute.") ||
				contains(trimmedLine, "specifies an invalid foreign repository name") ||
				contains(trimmedLine, "only specify one of the attributes item-type or data-type(s), not both") ||
				contains(trimmedLine, "specifies both component-item-type and component-data-type attributes") ||
				contains(trimmedLine, "specifies a repository attribute which is only valid for properties with") ||
				contains(trimmedLine, "has an invalid property-type") ||
				contains(trimmedLine, "has an invalid data type ") ||
				contains(trimmedLine, "is missing one of the component-data-type") ||
				contains(trimmedLine, "specifies an invalid item-descriptor") ||
				contains(trimmedLine, "specifies a value for both component-data-type and") ||
				contains(trimmedLine, "specifies an invalid value for the component-data-type attribute") ||
				contains(trimmedLine, "specifies an invalid item-type") ||
				contains(trimmedLine, "is improperly defined according to") ||
				contains(trimmedLine, ".  Using default property editor.") ||
				(contains(trimmedLine, "insert,update,delete") && contains(trimmedLine, "but does not refer to another item.")) ||
				(contains(trimmedLine, "insert,update,delete") && contains(trimmedLine, "but does not refer to another item.")) ||
				(contains(trimmedLine, "delete,insert") && contains(trimmedLine, "and refers to a item which has a property that refers back")) ||
				contains(trimmedLine, "All entries should be insert,update or delete.") ||
				contains(trimmedLine, "specifies a column-name property but is not inside of a table tag.") ||
				contains(trimmedLine, "specifies the group attribute but is not defined inside of a table tag") ||
				contains(trimmedLine, "specifies the default attribute but is not a scalar property.") ||
				contains(trimmedLine, "is a scalar property but is defined in a table tag with type=") ||
				contains(trimmedLine, "is a set but also specifies a multi-column-name") ||
				contains(trimmedLine, " is missing the multi-column-name attribute.") ||
				contains(trimmedLine, " must have either a component-item-type or component-data-type attribute.") ||
				contains(trimmedLine, "is a multi-valued property defined in a table that does not have type=") ||
				contains(trimmedLine, "sets a cache-mode that is not supported on property tags") ||
				contains(trimmedLine, "has some option tags which set the code value and others which do not set it explicitly") ||
				contains(trimmedLine, "specifies a code value which is not a valid integer.") ||
				contains(trimmedLine, "specifies an option code or value more than once:") ||
				contains(trimmedLine, "already has an attribute tag with name") ||
				contains(trimmedLine, "specifies an invalid data-type for an attribute tag") ||
				contains(trimmedLine, "specifies an invalid value for an attribute tag.") ||
				contains(trimmedLine, "Detailed error: ") ||
				contains(trimmedLine, "is not a valid data-type.") ||
				contains(trimmedLine, " could not be converted to the type ") ||
				contains(trimmedLine, "attribute with an invalid bean attribute.") ||
				contains(trimmedLine, "has an attribute with a null bean value ") ||
				contains(trimmedLine, "item-descriptor tag does not have a valid name:") ||
				contains(trimmedLine, "You have two item-descriptor tags with default=") ||
				(contains(trimmedLine, "in item-descriptor") && contains(trimmedLine, "You have two properties called ")) ||
				contains(trimmedLine, "Error trying to set an id generator high water mark:") ||
				contains(trimmedLine, " is an illegal value for the sub-type-property.") ||
				contains(trimmedLine, " has two id properties specified.") ||
				contains(trimmedLine, "Specify either value or bean, but not both.") ||
				contains(trimmedLine, "so the data-type attribute is not meaningful when ") ||
				contains(trimmedLine, "Invalid tag value: ") ||
				contains(trimmedLine, "Invalid composite format for repository ID:") ||
				contains(trimmedLine, "This item type does not support composite repository IDs:") ||
				contains(trimmedLine, "You specified both attributes id-column-name and id-column-names for table") ||
				contains(trimmedLine, "You must specify either id-column-name or id-column-names for table element") ||
				contains(trimmedLine, "You specified both attributes id-space-name and id-space-names for descriptor") ||
				contains(trimmedLine, "The parsed ID has values that do not correspond to the configured id-space-names:") ||
				contains(trimmedLine, "was specified with multiple columns. It will be treated as a read-only property") ||
				contains(trimmedLine, "was specified with multiple columns. It must either share all or none of") ||
				contains(trimmedLine, "Failed to add item to repository:") ||
				(contains(trimmedLine, "must both be versioning.") && contains(trimmedLine, "Your item-descriptor definitions for")) ||
				contains(trimmedLine, "Please specify the desired range when calling") ||
				contains(trimmedLine, "This repository may not yet be properly initialized.") ||

				contains(trimmedLine, "You must specify an XML configuration template file") ||
				contains(trimmedLine, "You must specify a repository") ||
				contains(trimmedLine, "You must specify an XMLTools object") ||
				contains(trimmedLine, "Secured repository failed to start") ||
				contains(trimmedLine, "There are no secured-repository-template elements") ||
				contains(trimmedLine, "Invalid/unknown identity:") ||
				contains(trimmedLine, "Invalid/unknown access right:") ||
				contains(trimmedLine, "Invalid/unknown owner identity:") ||
				contains(trimmedLine, "Invalid access control list:") ||
				contains(trimmedLine, "An item descriptor name must be specified") ||
				contains(trimmedLine, "is not a configured item descriptor of the repository") ||
				contains(trimmedLine, "A property name must be specified") ||
				contains(trimmedLine, "is not a configured property of the repository item") ||
				contains(trimmedLine, "An error occurred while evaluating function") ||
				contains(trimmedLine, "No function is mapped to the name") ||
				contains(trimmedLine, "An error occurred while parsing custom action attribute") ||
				contains(trimmedLine, "No such implicit object") ||
				contains(trimmedLine, "An exception occurred while trying to compare a value of") ||
				contains(trimmedLine, "An error occurred obtaining the indexed property value of an") ||
				contains(trimmedLine, "Unable to find a value for name") ||
				contains(trimmedLine, "An error occurred calling equals() on an object of type") ||
				contains(trimmedLine, "An error occurred applying operator") ||
				contains(trimmedLine, "Unable to parse value ") ||
				contains(trimmedLine, "but there is no PropertyEditor for that type") ||
				contains(trimmedLine, "An exception occurred trying to convert String") ||
				contains(trimmedLine, "Attempt to coerce ") ||
				contains(trimmedLine, "threw an exception in its toString()") ||
				contains(trimmedLine, "Unable to find a value for") ||
				contains(trimmedLine, "An exception occurred while trying to ") ||
				contains(trimmedLine, "that value cannot be converted to an integer.") ||
				contains(trimmedLine, "operator may not be null") ||
				contains(trimmedLine, "Attempt to apply a null index to the") ||
				contains(trimmedLine, "An error occurred while getting property") ||
				contains(trimmedLine, "does not have a public getter method") ||
				contains(trimmedLine, "Attempt to get property") ||
				contains(trimmedLine, "A null expression string may not be passed to the") ||
				contains(trimmedLine, "An Exception occurred getting the BeanInfo for class") ||
				contains(trimmedLine, "An attempt was made to register two Home") ||
				contains(trimmedLine, "Failed to delete file") ||
				contains(trimmedLine, "Did not successfully copy file") ||
				contains(trimmedLine, "IOException received while copying or checking file") ||
				contains(trimmedLine, "Error received while performing operation") ||
				contains(trimmedLine, "Unable to extract data from cache data file") ||
				contains(trimmedLine, "IOException received while operating on cache data file") ||
				contains(trimmedLine, "Incorrect format for checksum file cache line") ||
				contains(trimmedLine, "Checksum cache file nonexistent during load.  If you see this warning repeatedly") ||
				contains(trimmedLine, "Null file passed to checksum cache") ||
				contains(trimmedLine, "File System is immutable. Cannot create new file.") ||
				contains(trimmedLine, "Invalidate transAttribute value") ||
				contains(trimmedLine, "Registry is Not Defined") ||
				contains(trimmedLine, "Missing the Security Configuration") ||
				contains(trimmedLine, "Missing Default Access Control List") ||
				contains(trimmedLine, "unknown JDBC types for property") ||

				// from /atg/nucleus/servlet/NucleusServletResources.properties
				contains(trimmedLine, "***** ERROR:  Could not get ServletContext for atg_bootstrap.war") ||
				contains(trimmedLine, "Failing NucleusServlet startup") ||
				contains(trimmedLine, "Nucleus was not properly initialized") ||
				contains(trimmedLine, "RuntimeException caught by proxy servlet") ||
				contains(trimmedLine, "NucleusServlet: Could not load class") ||
				contains(trimmedLine, "NucleusServlet: Could not instantiate class") ||
				contains(trimmedLine, "NucleusServlet: IllegalAccessException while invoking initializer") ||
				contains(trimmedLine, "NucleusServlet: NoSuchMethodException while invoking initializer") ||
				contains(trimmedLine, "NucleusServlet: InvocationTargetException while invoking initializer") ||
				contains(trimmedLine, "Cannot determine Nucleus configpath root.") ||
				contains(trimmedLine, "NucleusServlet: can't set init properties") ||
				contains(trimmedLine, "ERROR: no system nucleus after launching") ||
				contains(trimmedLine, "NucleusServlet: can't set init properties") ||
				contains(trimmedLine, "Nucleus failed to start") ||
				contains(trimmedLine, "Error spawning a local nucleus for context") ||
				contains(trimmedLine, "Error stopping nucleus") ||
				contains(trimmedLine, "Could not get the class for the JBoss TransactionManagerFactory") ||
				contains(trimmedLine, "does not have a method named") ||
				contains(trimmedLine, "Could not get the class for the IBM TransactionManagerFactory") ||
				contains(trimmedLine, "Error encountered while initializing Nucleus servlet") ||

				contains(trimmedLine, "adding form exception:") ||
				contains(trimmedLine, "SystemErr     R 	at ") ||

				contains(trimmedLine, "An error occurred at line:") ||
				contains(trimmedLine, "Generated servlet error:") ||
				contains(trimmedLine, "could not be found. Please ensure that the JNDI name in the weblogic-ejb-jar.xml") ||
				startsWith(trimmedLine, "Caught exception in ") ||
				contains(trimmedLine, "Marking this deployment as FAILED") ||
				contains(trimmedLine, "Invalid object name '") ||
				contains(trimmedLine, "Can't find element with id=") ||
				contains(trimmedLine, "*** unable to find GSARepository component:") ||
				contains(trimmedLine, "Nested exception is:") ||
				contains(trimmedLine, "OutOfMemoryException") ||
				endsWith(trimmedLine, " cannot be resolved") ||
				startsWith(trimmedLine, "Error:") ||
				startsWith(trimmedLine, "log4j:ERROR") ||
				contains(trimmedLine, "ERROR:") ||
				startsWith(trimmedLine, "Nested Exception is") ||
				contains(trimmedLine, "message = Deployment Failed time") ||
				contains(trimmedLine, "atg.deployment.DeploymentFailure@") ||
				contains(trimmedLine, "has more than one primary table defined") ||
				contains(trimmedLine, "specifies a component-item-type or component-data-type attribute for a single value property") ||
				contains(trimmedLine, "has super-type product but no sub-type attribute") ||
				startsWith(trimmedLine, "Stacktrace:") ||
				contains(trimmedLine, "Ensure that the first WebLogic Server is completely shutdown and restart the server") ||
				contains(trimmedLine, "The WebLogic Server did not start up properly.") ||
				contains(trimmedLine, "[STDOUT] java.lang.OutOfMemoryError") ||
				contains(trimmedLine, "[STDOUT] AxisFault") ||
				startsWith(trimmedLine, "faultCode:") ||
				endsWith(trimmedLine, "faultSubcode:") ||
				endsWith(trimmedLine, "faultActor:") ||
				startsWith(trimmedLine, "faultString:") ||
				startsWith(trimmedLine, "AxisFault") ||
				startsWith(trimmedLine, "Fault occurred in processing") ||
				endsWith(trimmedLine, "faultNode:") ||
				endsWith(trimmedLine, "faultDetail:"))
				{
					return true;
				}
				return false;
}

/*
 *	these booleans are for specific conditions that often happen in log files. for instance,
 *	you may see the following in a log:
 *		[4/6/07 9:58:53:799 EDT] 0000000a SystemOut     O /atg/dynamo/security/AdminSqlRepository       SQL Statement Failed: [++SQLInsert++]
 *		INSERT INTO das_account(account_name,type,description,lastpwdupdate)
 *		VALUES(?,?,?,?)
 *		-- Parameters --
 *		p[1] = {pd} tools-integrations-privilege (java.lang.String)
 *		p[2] = {pd: type} 4 (java.lang.Integer)
 *		p[3] = {pd: description} Tools: Integrations (java.lang.String)
 *		p[4] = {pd: lastPasswordUpdate} 2007-04-06 09:58:51.221 (java.sql.Timestamp)
 *		[--SQLInsert--]
 *	Everything past the first line is printed to the screen via SOP. If the below method encounters
 *	"-- Parameters --" it's a dead give-away that we've hit this scenario. In that instance,
 *	isSQLDebug is set to true and the subsequent lines are colored red until "--SQLInsert--"
 *	is encountered, which signals the end of that condition. Had this logic not been there, those
 *	SOP's would have been colored as "other" lines. jbossObjectNameDump and the rest of the booleans
 *	are for similar logic. it sure would be nice if each line were prefixed with the appropriate line type
 */

bool jbossObjectNameDump = false;
bool jbossTableDebug = false;
bool isWebSphere = false;
bool isJBoss = false;
bool isWebLogic = false;
bool isSQLDebug = false;
bool isClassPath = false;
bool isConfigPath = false;
bool isJBossInterceptorChain = false;
bool isJBossNamingFactory = false;
bool isWSError = false;
bool isThreadDump = false;

/*
 *	given a specific line in a log file along with arrays containing previous lines and their
 *  types, this method returns its type. there isn't any magic to this. i've poured over
 *  hundreds of thousands of log files from JBoss, DAS, WebSphere, and WebLogic to determine
 *  their individual patterns. unfortunately, not everybody uses log4j and even when log4j is
 *  used, it's often mis-configured or there are weird SOP's and that kind of thing. this
 *  method is pretty much a whole bunch of if/else's. it returns one of six line types:
 *  INFO_LINE, WARNING_LINE, DEBUG_LINE, ERROR_LINE, OTHER_LINE, and NUCLEUS_LINE
 */
int determineLineType(string line, string trimmedLine, int previousLineTypes[], string previousLines[], string previousLinesTrimmed[])
{
	// most recent items are in the first positions of the array
	int previousLineType = previousLineTypes[0];
	string trimmedPreviousLine = previousLinesTrimmed[0];

	// I'm assuming here that this is always the last thread in the dump. If so, break out of loop
	if (contains(trimmedLine, "VM Periodic Task Thread") || contains(trimmedLine, "Suspend Checker Thread"))
	{
		isThreadDump=false;
		return INFO_LINE;
	}

	if (startsWith(trimmedLine, "Full thread dump Java HotSpot"))
	{
		isThreadDump=true;
	}

	if (isThreadDump)
	{
		return INFO_LINE;
	}

	// is output from websphere?
	if (
			!isJBoss &&      // once app server type is determined, skip the check for subsequent lines
			!isWebSphere &&
			!isWebLogic &&
			(
				contains(trimmedLine, "WebSphere Platform") ||
				contains(trimmedLine, "ATG starting on IBM WebSphere")
			)
		)
	{
		isWebSphere = true;
	}
	// or is it from jboss?
	else if (
				!isWebSphere &&
				!isJBoss &&
				!isWebLogic &&
				(
					contains(trimmedLine, "Starting JBoss") ||
					contains(trimmedLine, " DEBUG [org.jboss") ||
					contains(trimmedLine, "org.jboss.system") ||
					contains(trimmedLine, "org.jboss.logging")
				)
			)
	{
		isJBoss = true;
	}
	else if (
				!isWebSphere &&
				!isJBoss &&
				!isWebLogic &&
				(
						startsWith(trimmedLine, "WebLogic Server") ||
						contains(trimmedLine, "WLS Kernel")
				)
			)
	{
		isWebLogic=true;
	}

	/*
		// jbossObjectNameDump is a boolean representing whether the current line is the first
		// in the example below. If so, subsequent lines should be colored debug to match, as
		// they're really part of the same statement
		2006-06-26 14:34:47,671 DEBUG [org.jboss.system.ServiceController] Creating dependent components for: jboss:service=proxyFactory,target=ClientUserTransaction dependents are: [ObjectName: jboss:service=ClientUserTransaction
		State: CONFIGURED
		I Depend On:
			jboss:service=proxyFactory,target=ClientUserTransactionFactory
			jboss:service=proxyFactory,target=ClientUserTransaction
		]
	*/
	if (
			!jbossObjectNameDump &&
			contains(trimmedLine, "[ObjectName:") &&
			contains(trimmedLine, " DEBUG [") &&
			contains(trimmedLine, "jboss")
		)
	{
		jbossObjectNameDump = true;
	}
	else if (
				jbossObjectNameDump &&
				endsWith(trimmedLine, "]") &&
				!contains(trimmedLine, " INFO  [STDOUT]")
			)
	{
		jbossObjectNameDump = false;
		return DEBUG_LINE;
	}
	if (jbossObjectNameDump)
	{
		return DEBUG_LINE;
	}

	/*
		2007-04-11 16:59:02,474 DEBUG [org.jboss.services.binding.AttributeMappingDelegate] setAttribute, name='Properties', text=java.naming.factory.initial=org.jnp.interfaces.NamingContextFactory
						java.naming.factory.url.pkgs=org.jboss.naming:org.jnp.interfaces
						java.naming.provider.url=0.0.0.0:1200
						jnp.disableDiscovery=false
						jnp.partitionName=DefaultPartition
						jnp.discoveryGroup=230.0.0.4
						jnp.discoveryPort=1102
						jnp.discoveryTTL=16
						jnp.discoveryTimeout=5000
						jnp.maxRetries=1, value={java.naming.factory.initial=org.jnp.interfaces.NamingContextFactory, jnp.partitionName=DefaultPartition, jnp.discoveryTimeout=5000, jnp.discoveryGroup=230.0.0.4, jnp.disableDiscovery=false, java.naming.provider.url=0.0.0.0:1200, java.naming.factory.url.pkgs=org.jboss.naming:org.jnp.interfaces, jnp.maxRetries=1, jnp.discoveryPort=1102, jnp.discoveryTTL=16}
		2007-04-11 16:59:02,474 DEBUG [org.jboss.system.ServiceCreator] About to create bean: jboss.mq:service=ServerSessionPoolMBean,name=StdJMSPool with code: org.jboss.jms.asf.ServerSessionPoolLoader
	*/
	if (
			!isJBossNamingFactory &&
			isJBoss &&
			endsWith(trimmedLine, "NamingContextFactory") &&
			contains(trimmedLine, " DEBUG [")
		)
	{
		isJBossNamingFactory = true;
	}
	else if (
				isJBossNamingFactory &&
				isJBoss &&
				contains(trimmedLine, "[")
			)
	{
		isJBossNamingFactory = false;
		return DEBUG_LINE;
	}
	if (isJBossNamingFactory)
	{
		return DEBUG_LINE;
	}

	if (
			!jbossTableDebug &&
			endsWith(trimmedLine, "(") &&
			contains(trimmedLine, " DEBUG [") &&
			contains(trimmedLine, "table") &&
			contains(trimmedLine, "jboss")
		)
	{
		jbossTableDebug = true;
	}
	else if (jbossTableDebug && startsWith(trimmedLine, ")"))
	{
		jbossTableDebug = false;
		return DEBUG_LINE;
	}

	if (jbossTableDebug)
	{
		return DEBUG_LINE;
	}

	/*
		2007-04-11 16:58:57,359 INFO  [org.jboss.cache.factories.InterceptorChainFactory] interceptor chain is:
		class org.jboss.cache.interceptors.CallInterceptor
		class org.jboss.cache.interceptors.PessimisticLockInterceptor
		class org.jboss.cache.interceptors.UnlockInterceptor
		class org.jboss.cache.interceptors.ReplicationInterceptor
		class org.jboss.cache.interceptors.TxInterceptor
		class org.jboss.cache.interceptors.CacheMgmtInterceptor
	*/

		if (
				isJBoss &&
				!isJBossInterceptorChain &&
				(
					endsWith(trimmedLine, "interceptor chain is:")
				)
		)
	{
		isJBossInterceptorChain = true;
	}
	else if (
				isJBoss &&
				isJBossInterceptorChain &&
				!startsWith(trimmedLine, "class org.")
			)
	{
		isJBossInterceptorChain = false;
	}

	if (isJBossInterceptorChain)
	{
		return INFO_LINE;
	}
	/*
		[4/6/07 9:58:53:799 EDT] 0000000a SystemOut     O /atg/dynamo/security/AdminSqlRepository       SQL Statement Failed: [++SQLInsert++]
		INSERT INTO das_account(account_name,type,description,lastpwdupdate)
		VALUES(?,?,?,?)
		-- Parameters --
		p[1] = {pd} tools-integrations-privilege (java.lang.String)
		p[2] = {pd: type} 4 (java.lang.Integer)
		p[3] = {pd: description} Tools: Integrations (java.lang.String)
		p[4] = {pd: lastPasswordUpdate} 2007-04-06 09:58:51.221 (java.sql.Timestamp)
		[--SQLInsert--]
	*/

		if (
				!isSQLDebug &&
				(
					contains(trimmedLine, "SQL Statement Failed: [++SQLInsert++]") ||
					contains(trimmedLine, "SQL Statement Failed: [++SQLUpdate++]") ||
					contains(trimmedLine, "SQL Statement Failed: [++SQLDelete++]") ||
					contains(trimmedLine, "SQL Statement Failed: [++SQLSelect++]")
				)
		)
	{
		isSQLDebug = true;
	}
	else if (
				isSQLDebug &&
				(
					contains(trimmedLine, "[--SQLInsert--]") ||
					contains(trimmedLine, "[--SQLUpdate--]") ||
					contains(trimmedLine, "[--SQLDelete--]") ||
					contains(trimmedLine, "[--SQLSelect--]")
				)
			)
	{
		isSQLDebug = false;
		return ERROR_LINE;
	}

	if (isSQLDebug)
	{
		return ERROR_LINE;
	}

/*
	2007-04-10 18:22:14,593 ERROR [com.primus.routing.RoutedServerRequestOnly] Request to server URL: http://localhost:6072/AEXmlService/ failed on the below soap request
	. More details will follow. <?xml version="1.0"?><query version="4.0" responseNumberSettings="doc5,perDoc3,ans5,perAns1,f2,s5,d0,t5" minScore="300" sorting="SCORE" RQ
	Text="ANSWER" exclusion="" client="AE Web Client" debug="false" QUID="123"><question>Are you there?</question><clustername></clustername><documentSets><and><or><set s
	ubdirs="true">/</set></or><or><set subdirs="true">/</set></or></and></documentSets><parserOptions><context>company</context><context>computer</context><usageI>person<
	/usageI><usageWe>person</usageWe><usageYou>person</usageYou><language>english</language></parserOptions></query>
	2007-04-10 18:22:14,593 ERROR [com.primus.routing.api.RoutingServerEJBBean] Request for
			Cluster: DefaultCluster
			Group: 1
			Server: 80, http://localhost:6072/AEXmlService/
	failed on request (stack trace to follow):
	<?xml version="1.0"?><query version="4.0" responseNumberSettings="doc5,perDoc3,ans5,perAns1,f2,s5,d0,t5" minScore="300" sorting="SCORE" RQText="ANSWER" exclusion="" c
	lient="AE Web Client" debug="false" QUID="123"><question>Are you there?</question><clustername></clustername><documentSets><and><or><set subdirs="true">/</set></or><o
	r><set subdirs="true">/</set></or></and></documentSets><parserOptions><context>company</context><context>computer</context><usageI>person</usageI><usageWe>person</usa
	geWe><usageYou>person</usageYou><language>english</language></parserOptions></query>
*/
if (
	!isWSError &&
	(
		endsWith(trimmedLine, "] Request for") &&
		contains(trimmedLine, " ERROR [")
	)
		)
	{
		isWSError = true;
	}
	else if (
				isWSError &&
				(
					contains(trimmedLine, "[") ||
					contains(trimmedLine, "]")
				)
			)
	{
		isWSError = false;
	}

	if (isWSError)
	{
		return ERROR_LINE;
	}


	/*
		[4/9/07 11:22:23:683 EDT] 0000004c NucleusServle I atg.nucleus.servlet.NucleusServlet initBigEarNucleus CLASSPATH=

			C:\IBM\WebSphere\AppServer\java\lib,
			C:\IBM\WebSphere\AppServer\java\lib\dt.jar,
			C:\IBM\WebSphere\AppServer\java\lib\htmlconverter.jar,
			C:\IBM\WebSphere\AppServer\java\lib\tools.jar,
			C:\IBM\WebSphere\AppServer\profiles\20063\classes,
			C:\IBM\WebSphere\AppServer\lib,
			C:\IBM\WebSphere\AppServer\lib\AMJACCProvider.jar,
			C:\IBM\WebSphere\AppServer\lib\DDParser5.jar,
			C:\IBM\WebSphere\AppServer\lib\EJBCommandTarget.jar,
			C:\IBM\WebSphere\AppServer\lib\IVTClient.jar,
			C:\IBM\WebSphere\AppServer\lib\PDWASAuthzManager.jar,
			C:\IBM\WebSphere\AppServer\lib\UDDICloudscapeCreate.jar,
			C:\IBM\WebSphere\AppServer\lib\UDDIValueSetTools.jar,
			C:\IBM\WebSphere\AppServer\lib\WebSealTAIwas6.jar,
	*/
	if (
			!isClassPath &&
			endsWith(trimmedLine, "CLASSPATH=") &&
			(
				startsWith(trimmedLine, "C:") ||
				startsWith(trimmedLine, "D:") ||
				startsWith(trimmedLine, "/") ||
				startsWith(trimmedLine, "vfs=") ||
				startsWith(trimmedLine, "ATG-Data") ||
				startsWith(trimmedLine, "")
			)
		)
	{
		isClassPath = true;
		return INFO_LINE;
	}
	else if (isClassPath)
	{
		if (
				startsWith(trimmedLine, "C:") ||
				startsWith(trimmedLine, "D:") ||
				startsWith(trimmedLine, "/") ||
				startsWith(trimmedLine, "vfs=") ||
				startsWith(trimmedLine, "ATG-Data")
			)
		{
			return INFO_LINE;
		}
		else
		{
			isClassPath = false;
		}
	}


	/*
		CONFIGPATH=

			C:\IBM\WebSphere\AppServer\profiles\20063\installedApps\gopricahpNode01Cell\ATGApp.ear\atg_bootstrap.war\WEB-INF\ATG-INF\DAS\config\config.jar,
			C:\IBM\WebSphere\AppServer\profiles\20063\installedApps\gopricahpNode01Cell\ATGApp.ear\atg_bootstrap.war\WEB-INF\ATG-INF\DAS\config\oca-ldap.jar,
			C:\IBM\WebSphere\AppServer\profiles\20063\installedApps\gopricahpNode01Cell\ATGApp.ear\atg_bootstrap.war\WEB-INF\ATG-INF\DAS-UI\config\uiconfig.jar,
			C:\IBM\WebSphere\AppServer\profiles\20063\installedApps\gopricahpNode01Cell\ATGApp.ear\atg_bootstrap.war\WEB-INF\ATG-INF\DPS\config\targeting.jar,
			C:\IBM\WebSphere\AppServer\profiles\20063\installedApps\gopricahpNode01Cell\ATGApp.ear\atg_bootstrap.war\WEB-INF\ATG-INF\DPS\config\oca-cms.jar,
			C:\IBM\WebSphere\AppServer\profiles\20063\installedApps\gopricahpNode01Cell\ATGApp.ear\atg_bootstrap.war\WEB-INF\ATG-INF\DPS\config\oca-html.jar,
			C:\IBM\WebSphere\AppServer\profiles\20063\installedApps\gopricahpNode01Cell\ATGApp.ear\atg_bootstrap.war\WEB-INF\ATG-INF\DPS\config\oca-xml.jar,
			C:\IBM\WebSphere\AppServer\profiles\20063\installedApps\gopricahpNode01Cell\ATGApp.ear\atg_bootstrap.war\WEB-INF\ATG-INF\DPS\config\userprofiling.jar,
			C:\IBM\WebSphere\AppServer\profiles\20063\installedApps\gopricahpNode01Cell\ATGApp.ear\atg_bootstrap.war\WEB-INF\ATG-INF\DPS\config\profile.jar,
			C:\IBM\WebSphere\AppServer\profiles\20063\installedApps\gopricahpNode01Cell\ATGApp.ear\atg_bootstrap.war\WEB-INF\ATG-INF\DSS\config\config.jar,
			C:\IBM\WebSphere\AppServer\profiles\20063\installedApps\gopricahpNode01Cell\ATGApp.ear\atg_bootstrap.war\WEB-INF\ATG-INF\DafEar\base\config\dafconfig.jar,
			C:\IBM\WebSphere\AppServer\profiles\20063\installedApps\gopricahpNode01Cell\ATGApp.ear\atg_bootstrap.war\WEB-INF\ATG-INF\DafEar\WebSphere\config\wsconfig.jar,
			C:\IBM\WebSphere\AppServer\profiles\20063\installedApps\gopricahpNode01Cell\ATGApp.ear\atg_bootstrap.war\WEB-INF\ATG-INF\home\localconfig,
			ATG-Data\localconfig
	*/
	if (!isConfigPath && endsWith(trimmedLine, "CONFIGPATH="))
	{
		isConfigPath = true;
		return INFO_LINE;
	}
	else if (isConfigPath)
	{
		if (
				startsWith(trimmedLine, "C:") ||
				startsWith(trimmedLine, "D:") ||
				startsWith(trimmedLine, "/") ||
				startsWith(trimmedLine, "vfs=") ||
				startsWith(trimmedLine, "ATG-Data")
			)
		{
			return INFO_LINE;
		}
		else
		{
			isConfigPath = false;
		}
	}


	if (
				contains(trimmedLine, "Throwable while attempting to get a new connection") ||
				contains(trimmedLine, "Exception destroying ManagedConnection") ||
				contains(trimmedLine, "ConcurrentUpdateException caught updating an item during a commit") ||
				contains(trimmedLine, "XAException: tx=") ||
				contains(trimmedLine, "] Failed to connect to ") ||
				contains(trimmedLine, "java.lang.NoClassDefFoundError") ||
				contains(trimmedLine, "Error registering request") ||
				contains(trimmedLine, "java.lang.NullPointerException") ||
				contains(trimmedLine, "Illegal access: this web application instance has been stopped already.")
		)
	{
		return ERROR_LINE;
	}
	else if (
				contains(trimmedLine, "Nucleus running") ||
				contains(trimmedLine, "Starting Nucleus") ||
				contains(trimmedLine, "Invoking custom Nucleus") ||
				contains(trimmedLine, "Nucleus shutting down") ||
				contains(trimmedLine, "Nucleus shutdown complete") ||
				contains(trimmedLine, "Nucleus not running")
			)
	{
		return NUCLEUS_LINE;
	}

	// DAS prefixes all lines with **** type
	else if (contains(trimmedLine, "**** Error"))
	{
		return ERROR_LINE;
	}
	else if (contains(trimmedLine, "**** info"))
	{
		return INFO_LINE;
	}
	else if (contains(trimmedLine, "**** debug"))
	{
		return DEBUG_LINE;
	}
	else if (contains(trimmedLine, "**** Debug"))
	{
		return DEBUG_LINE;
	}
	else if (contains(trimmedLine, "**** Warning"))
	{
		return WARNING_LINE;
	}

	// JBoss/log4j
	// if running JBoss without log4j correctly confirgured, everything from ATG will come through INFO  [STDOUT]
	else if (contains(trimmedLine, " INFO  [") && !contains(trimmedLine, "INFO  [STDOUT]"))
	{
		return INFO_LINE;
	}
	else if (contains(trimmedLine, " WARN  ["))
	{
		return WARNING_LINE;
	}
	else if (contains(trimmedLine, " ERROR ["))
	{
		return ERROR_LINE;
	}
	else if (contains(trimmedLine, " FATAL ["))
	{
		return ERROR_LINE;
	}


	// 2007-02-26 15:37:18,723 DEBUG [org.jboss.mq.pm.jdbc2.PersistenceManager] Could not create table with SQL: CREATE CACHED TABLE JMS_MESSAGES ( MESSAGEID INTEGER NOT NULL, DESTINATION VARCHAR(255) NOT NULL, TXID INTEGER, TXOP CHAR(1), MESSAGEBLOB OBJECT, PRIMARY KEY (MESSAGEID, DESTINATION) )
	else if (contains(trimmedLine, "Could not create table with SQL"))
	{
		return ERROR_LINE;
	}
	else if (contains(trimmedLine, " DEBUG ["))
	{
		return DEBUG_LINE;
	}

	// from tomcat
	else if (contains(trimmedLine, "{INFO}"))
	{
		return INFO_LINE;
	}
	else if (contains(trimmedLine, "{CONFIG}"))
	{
		return INFO_LINE;
	}
	else if (contains(trimmedLine, "{WARN}"))
	{
		return WARNING_LINE;
	}
	else if (contains(trimmedLine, "{ERROR}"))
	{
		return ERROR_LINE;
	}
	else if (contains(trimmedLine, "{FATAL}"))
	{
		return ERROR_LINE;
	}
	else if (contains(trimmedLine, "{DEBUG}"))
	{
		return DEBUG_LINE;
	}

	else if (contains(trimmedLine, "info]"))
	{
		return INFO_LINE;
	}
	else if (contains(trimmedLine, "config]"))
	{
		return INFO_LINE;
	}
	else if (contains(trimmedLine, "warn]"))
	{
		return WARNING_LINE;
	}
	else if (contains(trimmedLine, "error]"))
	{
		return ERROR_LINE;
	}
	else if (contains(trimmedLine, "fatal]"))
	{
		return ERROR_LINE;
	}
	else if (contains(trimmedLine, "debug]"))
	{
		return DEBUG_LINE;
	}



	// weblogic
	else if (contains(trimmedLine, "<Notice>"))
	{
		return INFO_LINE;
	}
	else if (contains(trimmedLine, "<Info>"))
	{
		return INFO_LINE;
	}
	else if (contains(trimmedLine, "<Alert>"))
	{
		return WARNING_LINE;
	}
	else if (contains(trimmedLine, "<Warning>"))
	{
		return WARNING_LINE;
	}
	else if (contains(trimmedLine, "<Error>"))
	{
		return ERROR_LINE;
	}
	else if (contains(trimmedLine, "<Critical>"))
	{
		return ERROR_LINE;
	}
	else if (contains(trimmedLine, "<Emergency>"))
	{
		return ERROR_LINE;
	}
	else if (contains(trimmedLine, "<Debug>"))
	{
		return DEBUG_LINE;
	}

	// sometimes jboss prints XML messages in the form
	//	<mbean code="org.jboss.jms.asf.ServerSessionPoolLoader" name="jboss.mq:service=ServerSessionPoolMBean,name=StdJMSPool">
	//		<depends optional-attribute-name="XidFactory">jboss:service=XidFactory</depends>
	//		<attribute name="PoolName">StdJMSPool</attribute>
	//		<attribute name="PoolFactoryClass">
	//		   org.jboss.jms.asf.StdServerSessionPoolFactory
	//		</attribute>
	//   </mbean>
	// this little else if prevents org.jboss.jms.asf.StdServerSessionPoolFactory and other similar lines from being colored red
	else if (
				(
					startsWith (trimmedLine, "com.") ||
					startsWith (trimmedLine, "org.") ||
					startsWith (trimmedLine, "atg.") ||
					startsWith (trimmedLine, "jrockit.") ||
					startsWith (trimmedLine, "java.") ||
					startsWith (trimmedLine, "javax.") ||
					startsWith (trimmedLine, "webservices.") ||
					startsWith (trimmedLine, "sun.") ||
					startsWith (trimmedLine, "oracle.") ||
					startsWith (trimmedLine, "weblogic.")
				)
		&& startsWith(trimmedPreviousLine, "<")
		&& previousLineType == OTHER_LINE)
	{
		return OTHER_LINE;
	}

	/*
		2007-03-14 16:44:38,493 ERROR [org.apache.commons.modeler.Registry] Error registering jboss.web:type=RequestProcessor,worker=jk-8109,name=JkRequest248
		java.lang.SecurityException: MBeanTrustPermission(register) not implied by protection domain of mbean class: org.apache.commons.modeler.BaseModelMBean, pd: Protection
		Domain  (file:/export/nauuser/jboss-4.0.3SP1/server/node01/tmp/deploy/tmp62537commons-modeler.jar <no signer certificates>)
		org.jboss.mx.loading.UnifiedClassLoader3@6127da{ url=file:/export/nauuser/jboss-4.0.3SP1/server/node01/deploy/jbossweb-tomcat55.sar/ ,addedOrder=10}
		<no principals>
	*/
	else if (
				(
					startsWith (trimmedLine, "com.") ||
					startsWith (trimmedLine, "org.") ||
					startsWith (trimmedLine, "atg.") ||
					startsWith (trimmedLine, "jrockit.") ||
					startsWith (trimmedLine, "java.") ||
					startsWith (trimmedLine, "javax.") ||
					startsWith (trimmedLine, "sun.") ||
					startsWith (trimmedLine, "webservices.") ||
					startsWith (trimmedLine, "oracle.") ||
					startsWith (trimmedLine, "weblogic.")
				)
		&& endsWith(trimmedPreviousLine, ">)")
		&& previousLineType == ERROR_LINE)
	{
		return ERROR_LINE;
	}

	/*
	 * Colors the http://xml.apache.org/axis/ line
	 *			at org.apache.axis.client.Call.invoke(Call.java:2366)
	 *			at org.apache.axis.client.Call.invoke(Call.java:1812)
	 *			at com.atg.www.b2cblueprint.integrations.B2CBlueprintOMSIntegrationWS.B2CBlueprintOMSIntegrationWSBindingStub.orderSubmitToOMS(B2CBlueprintOMSIntegrationWSBindingStub.java:939)
	 *			at atg.projects.b2cblueprint.integrations.webservices.B2CBlueprintOMSIntegrationWSClient.orderSubmitToOMS(B2CBlueprintOMSIntegrationWSClient.java:100)
	 *			at atg.projects.b2cblueprint.integrations.order.B2CBlueprintOMSOrderSubmissionService.submitOrdersToOMS(B2CBlueprintOMSOrderSubmissionService.java:306)
	 *			at atg.projects.b2cblueprint.integrations.order.B2CBlueprintOMSOrderSubmissionService.performScheduledTask(B2CBlueprintOMSOrderSubmissionService.java:420)
	 *			at atg.service.scheduler.Scheduler$1handler.run(Scheduler.java:535)
	 *			{http://xml.apache.org/axis/}hostname:PPUTAPPA
	 *	09:41:52,093 INFO  [STDOUT] java.net.ConnectException: Connection timed out: connect
	 *	09:41:52,093 INFO  [STDOUT]     at org.apache.axis.AxisFault.makeFault(AxisFault.java:101)
	 *	09:41:52,093 INFO  [STDOUT]     at org.apache.axis.transport.http.HTTPSender.invoke(HTTPSender.java:154)
	 *	09:41:52,093 INFO  [STDOUT]     at org.apache.axis.strategies.InvocationStrategy.visit(InvocationStrategy.java:32)
	 */
	else if (
				(
					startsWith (trimmedLine, "{http://xml.apache.org/axis/}hostname:")
				)
		&& previousLineType == ERROR_LINE)
	{
		return ERROR_LINE;
	}

	/*
		these next series of if else statements are to properly color the following:
		2007-03-05 23:19:58,092 INFO  [atg.nucleus.servlet.NucleusServlet] ENVIRONMENT=
			atg.dynamo.home=/export/nauuser/jboss-4.0.3SP1/server/node01/./deploy/nauCommerce.ear/atg_bootstrap.war/WEB-INF/ATG-INF/home,
			atg.dynamo.root=/export/nauuser/jboss-4.0.3SP1/server/node01/./deploy/nauCommerce.ear/atg_bootstrap.war/WEB-INF/ATG-INF,
			atg.dynamo.server.home=/export/nauuser/jboss-4.0.3SP1/server/node01/./deploy/nauCommerce.ear/atg_bootstrap.war/WEB-INF/ATG-INF/home/servers/node01,
			atg.dynamo.versioninfo=ATGPlatform/2006.3,
			atg.dynamo.liveconfig=on,
			atg.dynamo.modules=nauCommerce,DafEar.Admin,DAS.WebServices,DCS.WebServices,DCS.AbandonedOrderServices,
			atg.dynamo.platformversion=2006.3,
			atg.dynamo.server.name=node01,
			atg.dynamo.display=:0.0,
			atg.license.read=true,
			atg.dynamo.daf=true
			dataDir=/export/nauuser/jboss-4.0.3SP1/bin/ATG-Data
			servername=node01
			standlone=true
		2007-03-05 23:20:00,083 INFO  [nucleusNamespace.DPSLicense] DPS is licensed to NAU - Production
	*/
	else if (
				(
					startsWith(trimmedLine, "atg.dynamo") ||
					startsWith(trimmedLine, "atg.license") ||
					startsWith(trimmedLine, "dataDir")  ||
					startsWith(trimmedLine, "servername")  ||
					startsWith(trimmedLine, "standlone")
				) &&
				endsWith(previousLinesTrimmed[0], "ENVIRONMENT=") &&
				previousLineType == INFO_LINE
			)
	{
		return INFO_LINE;
	}
	else if (
				(
					startsWith(trimmedLine, "atg.dynamo") ||
					startsWith(trimmedLine, "atg.license") ||
					startsWith(trimmedLine, "dataDir")  ||
					startsWith(trimmedLine, "servername")  ||
					startsWith(trimmedLine, "standlone")
				) &&
				endsWith(previousLinesTrimmed[1], "ENVIRONMENT=")  &&
				previousLineType == INFO_LINE
			)
	{
		return INFO_LINE;
	}
	else if (
				(
					startsWith(trimmedLine, "atg.dynamo") ||
					startsWith(trimmedLine, "atg.license") ||
					startsWith(trimmedLine, "dataDir")  ||
					startsWith(trimmedLine, "servername")  ||
					startsWith(trimmedLine, "standlone")
				) &&
				endsWith(previousLinesTrimmed[2], "ENVIRONMENT=") &&
				previousLineType == INFO_LINE
			)
	{
		return INFO_LINE;
	}
	else if (
				(
					startsWith(trimmedLine, "atg.dynamo") ||
					startsWith(trimmedLine, "atg.license") ||
					startsWith(trimmedLine, "dataDir")  ||
					startsWith(trimmedLine, "servername")  ||
					startsWith(trimmedLine, "standlone")
				) &&
				endsWith(previousLinesTrimmed[3], "ENVIRONMENT=") &&
				previousLineType == INFO_LINE
			)
	{
		return INFO_LINE;
	}
	else if (
				(
					startsWith(trimmedLine, "atg.dynamo") ||
					startsWith(trimmedLine, "atg.license") ||
					startsWith(trimmedLine, "dataDir")  ||
					startsWith(trimmedLine, "servername")  ||
					startsWith(trimmedLine, "standlone")
				) &&
				endsWith(previousLinesTrimmed[4], "ENVIRONMENT=") &&
				previousLineType == INFO_LINE
			)
	{
		return INFO_LINE;
	}
	else if (
				(
					startsWith(trimmedLine, "atg.dynamo") ||
					startsWith(trimmedLine, "atg.license") ||
					startsWith(trimmedLine, "dataDir")  ||
					startsWith(trimmedLine, "servername")  ||
					startsWith(trimmedLine, "standlone")
				) &&
				endsWith(previousLinesTrimmed[5], "ENVIRONMENT=") &&
				previousLineType == INFO_LINE
			)
	{
		return INFO_LINE;
	}
	else if (
				(
					startsWith(trimmedLine, "atg.dynamo") ||
					startsWith(trimmedLine, "atg.license") ||
					startsWith(trimmedLine, "dataDir")  ||
					startsWith(trimmedLine, "servername")  ||
					startsWith(trimmedLine, "standlone")
				) &&
				endsWith(previousLinesTrimmed[6], "ENVIRONMENT=") &&
				previousLineType == INFO_LINE
			)
	{
		return INFO_LINE;
	}
	else if (
				(
					startsWith(trimmedLine, "atg.dynamo") ||
					startsWith(trimmedLine, "atg.license") ||
					startsWith(trimmedLine, "dataDir")  ||
					startsWith(trimmedLine, "servername")  ||
					startsWith(trimmedLine, "standlone")
				) &&
				endsWith(previousLinesTrimmed[7], "ENVIRONMENT=") &&
				previousLineType == INFO_LINE
			)
	{
		return INFO_LINE;
	}
	else if (
				(
					startsWith(trimmedLine, "atg.dynamo") ||
					startsWith(trimmedLine, "atg.license") ||
					startsWith(trimmedLine, "dataDir")  ||
					startsWith(trimmedLine, "servername")  ||
					startsWith(trimmedLine, "standlone")
				) &&
				endsWith(previousLinesTrimmed[8], "ENVIRONMENT=") &&
				previousLineType == INFO_LINE
			)
	{
		return INFO_LINE;
	}
	else if (
				(
					startsWith(trimmedLine, "atg.dynamo") ||
					startsWith(trimmedLine, "atg.license") ||
					startsWith(trimmedLine, "dataDir")  ||
					startsWith(trimmedLine, "servername")  ||
					startsWith(trimmedLine, "standlone")
				) &&
				endsWith(previousLinesTrimmed[9], "ENVIRONMENT=") &&
				previousLineType == INFO_LINE
			)
	{
		return INFO_LINE;
	}
	else if (
				(
					startsWith(trimmedLine, "atg.dynamo") ||
					startsWith(trimmedLine, "atg.license") ||
					startsWith(trimmedLine, "dataDir")  ||
					startsWith(trimmedLine, "servername")  ||
					startsWith(trimmedLine, "standlone")
				) &&
				endsWith(previousLinesTrimmed[10], "ENVIRONMENT=") &&
				previousLineType == INFO_LINE
			)
	{
		return INFO_LINE;
	}
	else if (
				(
					startsWith(trimmedLine, "atg.dynamo") ||
					startsWith(trimmedLine, "atg.license") ||
					startsWith(trimmedLine, "dataDir")  ||
					startsWith(trimmedLine, "servername")  ||
					startsWith(trimmedLine, "standlone")
				) &&
				endsWith(previousLinesTrimmed[11], "ENVIRONMENT=") &&
				previousLineType == INFO_LINE
			)
	{
		return INFO_LINE;
	}
	else if (
				(
					startsWith(trimmedLine, "atg.dynamo") ||
					startsWith(trimmedLine, "atg.license") ||
					startsWith(trimmedLine, "dataDir")  ||
					startsWith(trimmedLine, "servername")  ||
					startsWith(trimmedLine, "standlone")
				) &&
				endsWith(previousLinesTrimmed[12], "ENVIRONMENT=") &&
				previousLineType == INFO_LINE
			)
	{
		return INFO_LINE;
	}
	else if (
				(
					startsWith(trimmedLine, "atg.dynamo") ||
					startsWith(trimmedLine, "atg.license") ||
					startsWith(trimmedLine, "dataDir")  ||
					startsWith(trimmedLine, "servername")  ||
					startsWith(trimmedLine, "standlone")
				) &&
				endsWith(previousLinesTrimmed[13], "ENVIRONMENT=") &&
				previousLineType == INFO_LINE
			)
	{
		return INFO_LINE;
	}

	else if (startsWith(trimmedLine, "SQL Statement Failed"))
	{
		return ERROR_LINE;
	}

	/*
		2007-04-11 16:58:59,553 DEBUG [org.jboss.util.naming.Util] link: Reference Class Name: javax.naming.LinkRef
		Type: LinkAddress
		Content: jmx/invoker/RMIAdaptor
	*/
	else if (
				(
					startsWith(trimmedLine, "Type: ") ||
					startsWith(trimmedLine, "Content: ")
				) &&
				(
					contains(previousLinesTrimmed[0], " DEBUG [") ||
					contains(previousLinesTrimmed[1], " DEBUG [")
				)
		)
	{
		return DEBUG_LINE;
	}

	/*
		Designed to catch stack trace stuff, but ignore for instance the java.naming in the XML from JBoss below
		2007-04-11 16:59:10,358 DEBUG [org.jboss.deployment.XSLSubDeployer] transformed into doc: <server>
		<mbean code='org.jboss.jms.jndi.JMSProviderLoader' name='jboss.mq:service=JMSProviderLoader,name=HAJNDIJMSProvider'>
		<attribute name='ProviderName'>DefaultJMSProvider</attribute>
		<attribute name='ProviderAdapterClass'>
			org.jboss.jms.jndi.JNDIProviderAdapter
			</attribute>
		<attribute name='FactoryRef'>XAConnectionFactory</attribute>
		<attribute name='QueueFactoryRef'>XAConnectionFactory</attribute>
		<attribute name='TopicFactoryRef'>XAConnectionFactory</attribute>
		<attribute name='Properties'>
			java.naming.factory.initial=org.jnp.interfaces.NamingContextFactory
			java.naming.factory.url.pkgs=org.jboss.naming:org.jnp.interfaces
			java.naming.provider.url=${jboss.bind.address:localhost}:1100
			jnp.disableDiscovery=false
			jnp.partitionName=${jboss.partition.name:DefaultPartition}
			jnp.discoveryGroup=${jboss.partition.udpGroup:230.0.0.4}
			jnp.discoveryPort=1102
			jnp.discoveryTTL=16
			jnp.discoveryTimeout=5000
			jnp.maxRetries=1
			</attribute>
		</mbean>
	*/
	else if (
				(
					contains(trimmedLine, "[STDOUT]     at") ||
					startsWith(trimmedLine, "at ") ||
					startsWith(trimmedLine, "org.") ||
					startsWith(trimmedLine, "(org.") ||
					startsWith(trimmedLine, "sun.") ||
					startsWith(trimmedLine, "(sun.") ||
					startsWith(trimmedLine, "atg.") ||
					startsWith(trimmedLine, "(atg.") ||
					startsWith(trimmedLine, "com.") ||
					startsWith(trimmedLine, "(com.") ||
					startsWith(trimmedLine, "jrockit.") ||
					startsWith(trimmedLine, "(jrockit.") ||
					startsWith(trimmedLine, "webservices.") ||
					startsWith(trimmedLine, "(webservices.") ||
					startsWith(trimmedLine, "java.") ||
					startsWith(trimmedLine, "(java.") ||
					startsWith(trimmedLine, "javax.") ||
					startsWith(trimmedLine, "(javax.") ||
					startsWith(trimmedLine, "oracle.") ||
					startsWith(trimmedLine, "(oracle.") ||
					startsWith(trimmedLine, "weblogic.") ||
					startsWith(trimmedLine, "(weblogic.") ||
					startsWith(trimmedLine, "<no ") ||
					startsWith(trimmedLine, "CAUSE:") ||
					startsWith(trimmedLine, "Exception in") ||
					startsWith(trimmedLine, "Caused by") ||
					startsWith(trimmedLine, "Symbol") ||
					startsWith(trimmedLine, "Location") ||
					startsWith(trimmedLine, "...stack") ||
					startsWith(trimmedLine, "... stack") ||
					startsWith(trimmedLine, ".... ") ||
					startsWith(trimmedLine, "....stack") ||
					startsWith(trimmedLine, ".... stack") ||
					startsWith(trimmedLine, "CAUGHT AT:") ||
					startsWith(trimmedLine, "CONTAINER:") ||
					startsWith(trimmedLine, "SOURCE EXCEPTION:") ||
					contains(trimmedLine, "nested exception is:")
				) && !contains(trimmedLine, "=")
			)
	{
		return ERROR_LINE;
	}

	/*
        at atg.servlet.WrappingRequestDispatcher.include(WrappingRequestDispatcher.java:94)
        at atg.taglib.dspjsp.IncludeTag.doEndTag(Unknown Source)
        ... 213 more
		SOURCE EXCEPTION:javax.servlet.ServletException: atg.adapter.gsa.GSARepository
	*/
	else if (contains(trimmedLine, "...") && contains(trimmedLine, "more"))
	{
		return ERROR_LINE;
	}

	// more stack trace stuff
	// [4/6/07 16:23:13:093 EDT] 00000050 WorkSpaceMana E   WKSP0019E: Error getting repository adapter com.ibm.ws.management.configarchive.ConfigArchiveRepositoryAdapter -- java.lang.ClassNotFoundException: com.ibm.ws.management.configarchive.ConfigArchiveRepositoryAdapter
	else if (
				contains(trimmedLine, "Exception:") &&
				(
					contains(trimmedLine, "com.") ||
					contains(trimmedLine, "org.") ||
					contains(trimmedLine, "atg.") ||
					contains(trimmedLine, "jrockit.") ||
					contains(trimmedLine, "java.") ||
					contains(trimmedLine, "oracle.") ||
					contains(trimmedLine, "webservices.") ||
					contains(trimmedLine, "javax.") ||
					contains(trimmedLine, "sun.") ||
					contains(trimmedLine, "weblogic.")
				)
		)
	{
		return ERROR_LINE;
	}

	// [4/6/07 10:18:37:237 EDT] 00000021 ServletWrappe E   SRVE0068E: Could not invoke the service() method on servlet DynamoProxyServlet. Exception thrown : java.lang.NullPointerException
	// INFO  [STDOUT]  at org.apache.jk.server.JkCoyoteHandler.invoke(JkCoyoteHandler.java:307)
	else if (
				contains(trimmedLine, "Exception thrown") &&
				(
					contains(trimmedLine, "com.") ||
					contains(trimmedLine, "org.") ||
					contains(trimmedLine, "atg.") ||
					contains(trimmedLine, "jrockit.") ||
					contains(trimmedLine, "webservices.") ||
					contains(trimmedLine, "java.") ||
					contains(trimmedLine, "sun.") ||
					contains(trimmedLine, "oracle.") ||
					contains(trimmedLine, "javax.") ||
					contains(trimmedLine, "weblogic.")
				)
		)
	{
		return ERROR_LINE;
	}
	else if (startsWith(trimmedLine, "C:") && previousLineType == ERROR_LINE && !isWebSphere)
	{
		return ERROR_LINE;
	}
	else if (startsWith(trimmedLine, "D:") && previousLineType == ERROR_LINE && !isWebSphere)
	{
		return ERROR_LINE;
	}
	else if (startsWith(trimmedLine, "SEVERE:"))
	{
		return ERROR_LINE;
	}
	else if (startsWith(trimmedLine, "XML parsing error:"))
	{
		return ERROR_LINE;
	}

	else if (startsWith(trimmedLine, ">") && previousLineType == ERROR_LINE)
	{
		return ERROR_LINE;
	}
	else if (startsWith(trimmedLine, ")") && previousLineType == ERROR_LINE)
	{
		return ERROR_LINE;
	}

	/*
		C:\bea\user_projects\domains\atg\.\myserver\.wlnotdelete\extract\myserver_StoreApp_storeApp.war\jsp_servlet\_checkout\__shipping.java:648: setDefault(java.lang.String) in atg.taglib.dspjsp.InputTagBase cannot be applied to (boolean)
				_dsp_input0.setDefault(true); //[ /checkout/shipping.jsp; Line: 42]
							^
		1 error
	*/
	else if (startsWith(trimmedLine, "^") && previousLineType == ERROR_LINE)
	{
		return ERROR_LINE;
	}
	else if (
				previousLineType == ERROR_LINE &&
				(
					contains(trimmedLine, ".java:") ||
					contains(trimmedLine, "Line:") ||
					contains(trimmedLine, "location:") ||
					contains(trimmedLine, "symbol  :") ||
					contains(trimmedLine, ".jsp;")
				)
			)
	{
		return ERROR_LINE;
	}
	else if (
				(
					endsWith(trimmedLine, " error") ||
					endsWith(trimmedLine, " errors")
				) &&
				previousLineType == ERROR_LINE)
	{
		return ERROR_LINE;
	}

	/*
		[3/26/07 11:32:32:231 EST] 0000001f SystemOut     O /atg/dynamo/security/AdminAccountManager    Failure trying to retrieve account scenarios-privilege  CONTAINER:atg.repository.RepositoryException; SOURCE:java.sql.SQLException: [IBM][SQLServer JDBC Driver][SQLServer]Invalid object name 'das_account'.
	*/
	else if (contains(trimmedLine, "CONTAINER:") && contains(trimmedLine, "SOURCE:"))
	{
		return ERROR_LINE;
	}

	/*
		the next 9 if/else statements are for this condition:
		2006-06-26 14:35:26,171 INFO  [STDOUT] **** Error
		2006-06-26 14:35:26,171 INFO  [STDOUT]
		2006-06-26 14:35:26,171 INFO  [STDOUT] Mon Jun 26 14:35:26 CDT 2006
		2006-06-26 14:35:26,171 INFO  [STDOUT]
		2006-06-26 14:35:26,171 INFO  [STDOUT] 1151350526171
		2006-06-26 14:35:26,171 INFO  [STDOUT]
		2006-06-26 14:35:26,171 INFO  [STDOUT] /
		2006-06-26 14:35:26,171 INFO  [STDOUT]
		2006-06-26 14:35:26,171 INFO  [STDOUT]  java.net.BindException: Address already in use: JVM_Bind
	*/
	else if (
				endsWith(trimmedLine, "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[0], "[STDOUT] **** Error"))
	{
		return ERROR_LINE;
	}
	else if (
				contains(trimmedLine, "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[0], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[1], "[STDOUT] **** Error"))
	{
		return ERROR_LINE;
	}
	else if (
				contains(trimmedLine, "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[0], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[1], "[STDOUT] **** Error"))
	{
		return ERROR_LINE;
	}
	else if (
				endsWith(trimmedLine, "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[0], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[1], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[2], "[STDOUT] **** Error"))
	{
		return ERROR_LINE;
	}
	else if (
				contains(trimmedLine, "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[0], "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[1], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[2], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[3], "[STDOUT] **** Error"))
	{
		return ERROR_LINE;
	}
	else if (
				endsWith(trimmedLine, "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[0], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[1], "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[2], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[3], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[4], "[STDOUT] **** Error"))
	{
		return ERROR_LINE;
	}
	else if (
				(endsWith(trimmedLine, "INFO  [STDOUT] /") ||
				endsWith(trimmedLine, "INFO  [STDOUT] ---")) &&
				endsWith(previousLinesTrimmed[0], "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[1], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[2], "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[3], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[4], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[5], "[STDOUT] **** Error"))
	{
		return ERROR_LINE;
	}
	else if (
				endsWith(trimmedLine, "INFO  [STDOUT]") &&
				(endsWith(previousLinesTrimmed[0], "INFO  [STDOUT] /") ||
				endsWith(previousLinesTrimmed[0], "INFO  [STDOUT] ---")) &&
				endsWith(previousLinesTrimmed[1], "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[2], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[3], "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[4], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[5], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[6], "[STDOUT] **** Error"))
	{
		return ERROR_LINE;
	}
	else if (
				contains(trimmedLine, "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[0], "INFO  [STDOUT]") &&
				(endsWith(previousLinesTrimmed[1], "INFO  [STDOUT] /") ||
				endsWith(previousLinesTrimmed[1], "INFO  [STDOUT] ---")) &&
				endsWith(previousLinesTrimmed[2], "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[3], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[4], "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[5], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[6], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[7], "[STDOUT] **** Error"))
	{
		return ERROR_LINE;
	}


   /*
	*	the next 9 if/else statements are for this condition:
	* 15:02:27,087 INFO  [STDOUT] **** Warning
	* 15:02:27,087 INFO  [STDOUT]
	* 15:02:27,087 INFO  [STDOUT] Tue Jan 22 15:02:27 CST 2008
	* 15:02:27,087 INFO  [STDOUT]
	* 15:02:27,087 INFO  [STDOUT] 1201035747087
	* 15:02:27,087 INFO  [STDOUT]
	* 15:02:27,087 INFO  [STDOUT] /
	* 15:02:27,088 INFO  [STDOUT]
	* 15:02:27,088 INFO  [STDOUT] PR#142511 HOTFIX: Created on 11/21/2007
	*/
	else if (
				endsWith(trimmedLine, "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[0], "[STDOUT] **** Warning"))
	{
		return WARNING_LINE;
	}
	else if (
				contains(trimmedLine, "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[0], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[1], "[STDOUT] **** Warning"))
	{
		return WARNING_LINE;
	}
	else if (
				contains(trimmedLine, "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[0], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[1], "[STDOUT] **** Warning"))
	{
		return WARNING_LINE;
	}
	else if (
				endsWith(trimmedLine, "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[0], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[1], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[2], "[STDOUT] **** Warning"))
	{
		return WARNING_LINE;
	}
	else if (
				contains(trimmedLine, "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[0], "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[1], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[2], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[3], "[STDOUT] **** Warning"))
	{
		return WARNING_LINE;
	}
	else if (
				endsWith(trimmedLine, "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[0], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[1], "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[2], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[3], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[4], "[STDOUT] **** Warning"))
	{
		return WARNING_LINE;
	}
	else if (
				(endsWith(trimmedLine, "INFO  [STDOUT] /") ||
				endsWith(trimmedLine, "INFO  [STDOUT] ---")) &&
				endsWith(previousLinesTrimmed[0], "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[1], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[2], "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[3], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[4], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[5], "[STDOUT] **** Warning"))
	{
		return WARNING_LINE;
	}
	else if (
				endsWith(trimmedLine, "INFO  [STDOUT]") &&
				(endsWith(previousLinesTrimmed[0], "INFO  [STDOUT] /") ||
				endsWith(previousLinesTrimmed[0], "INFO  [STDOUT] ---")) &&
				endsWith(previousLinesTrimmed[1], "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[2], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[3], "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[4], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[5], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[6], "[STDOUT] **** Warning"))
	{
		return WARNING_LINE;
	}
	else if (
				contains(trimmedLine, "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[0], "INFO  [STDOUT]") &&
				(endsWith(previousLinesTrimmed[1], "INFO  [STDOUT] /") ||
				endsWith(previousLinesTrimmed[1], "INFO  [STDOUT] ---")) &&
				endsWith(previousLinesTrimmed[2], "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[3], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[4], "INFO  [STDOUT]") &&
				contains(previousLinesTrimmed[5], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[6], "INFO  [STDOUT]") &&
				endsWith(previousLinesTrimmed[7], "[STDOUT] **** Warning"))
	{
		return WARNING_LINE;
	}


	/*
		This else/if coveres the third line here
		2006-06-26 14:35:25,906 INFO  [STDOUT] ---
		2006-06-26 14:35:25,906 INFO  [STDOUT]
		2006-06-26 14:35:25,906 INFO  [STDOUT] java.rmi.server.ExportException: Port already in use: 0; nested exception is:
				java.net.BindException: Address already in use: JVM_Bind
				at sun.rmi.transport.tcp.TCPTransport.listen(TCPTransport.java:243)
				at sun.rmi.transport.tcp.TCPTransport.exportObject(TCPTransport.java:178)
				at sun.rmi.transport.tcp.TCPEndpoint.exportObject(TCPEndpoint.java:382)
				at sun.rmi.transport.LiveRef.exportObject(LiveRef.java:116)
				at sun.rmi.server.UnicastServerRef.exportObject(UnicastServerRef.java:145)
				at sun.rmi.server.UnicastServerRef.exportObject(UnicastServerRef.java:129)
				at java.rmi.server.UnicastRemoteObject.exportObject(UnicastRemoteObject.java:275)
				at java.rmi.server.UnicastRemoteObject.exportObject(UnicastRemoteObject.java:178)
		....stack trace CROPPED after 10 lines.
	*/
	else if (
				endsWith(previousLinesTrimmed[1], "INFO  [STDOUT] ---") &&
				endsWith(previousLinesTrimmed[0], "INFO  [STDOUT]") &&
				contains(trimmedLine, "INFO  [STDOUT]") &&
				(
					contains(trimmedLine, "com.") ||
					contains(trimmedLine, "org.") ||
					contains(trimmedLine, "atg.") ||
					contains(trimmedLine, "jrockit.") ||
					contains(trimmedLine, "java.") ||
					contains(trimmedLine, "sun.") ||
					contains(trimmedLine, "webservices.") ||
					contains(trimmedLine, "oracle.") ||
					contains(trimmedLine, "javax.") ||
					contains(trimmedLine, "weblogic.")
				)
			)
	{
		return ERROR_LINE;
	}

	else if (
				(
					contains(trimmedLine, "com.") ||
					contains(trimmedLine, "INFO  [STDOUT] org.") ||
					contains(trimmedLine, "INFO  [STDOUT] atg.") ||
					contains(trimmedLine, "INFO  [STDOUT] jrockit.") ||
					contains(trimmedLine, "INFO  [STDOUT] sun.") ||
					contains(trimmedLine, "INFO  [STDOUT] java.") ||
					contains(trimmedLine, "INFO  [STDOUT] webservices.") ||
					contains(trimmedLine, "INFO  [STDOUT] oracle.") ||
					contains(trimmedLine, "INFO  [STDOUT] javax.") ||
					contains(trimmedLine, "INFO  [STDOUT] weblogic.")
				) &&
				contains(trimmedLine, "Exception") &&
				contains(trimmedLine, ":")
			)
	{
		return ERROR_LINE;
	}

	else if (
				contains(trimmedLine, "---- Begin backtrace for Nested Throwables") ||
				// 2007-04-10 18:22:15,606 INFO  [STDOUT]  at sun.net.www.http.HttpClient.openServer(Unknown Source)
				contains(trimmedLine, " INFO  [STDOUT] 	at ") ||
				// 2007-04-10 18:22:52,217 DEBUG [org.jnp.interfaces.NamingContext] Failed to connect to t3:1099
				contains(trimmedLine, "Invalid/unknown identity:")
			)
		{
			return ERROR_LINE;
		}

	else if (
				contains(trimmedLine, "recovered deployment status is either not from this server or does not match") ||
				contains(trimmedLine, "Test this hotfix on a staging or other non-production environment") ||
				contains(trimmedLine, " HOTFIX: DO NOT patch beyond ATG")
			)
	{
		return WARNING_LINE;
	}

	/*
	 * Prevents the tyupe and content lines from showing up in yellow. they should be debug lines
	 * 2007-04-11 16:58:00,193 DEBUG [org.jboss.util.naming.Util] link: Reference Class Name: javax.naming.LinkRef
	 * Type: LinkAddress
	 * Content: ConnectionFactory
    /*

	else if (
				isJBoss &&
				contains(previousLinesTrimmed[0], " DEBUG [") &&
				(
					startsWith(trimmedLine, "Type:") ||
					startsWith(trimmedLine, "Content:")
				)
			)
	{
		return ERROR_LINE;
	}
	/*
		2006-06-26 14:34:53,406 DEBUG [org.jboss.services.binding.ServiceBindingManager] applyServiceConfig, server:ports-lpa-base, serviceName:jboss.web:service=WebServer, config=ServiceConfig(name=jboss.web:service=WebServer), bindings=
		ServiceBinding [name=;hostName=<ANY>;bindAddress=localhost/127.0.0.1;port=8280]
	*/
	else if (
				isJBoss &&
				endsWith(previousLinesTrimmed[0], "bindings=") &&
				contains(previousLinesTrimmed[0], " DEBUG [") &&
				startsWith(trimmedLine, "ServiceBinding")
			)
	{
		return DEBUG_LINE;
	}


	/*
	 *	16:22:36,275 INFO  [STDOUT] Error while handling scheduled job J2EE Archive Directory Agent
	 *	16:22:36,285 INFO  [STDOUT]
	 *	16:22:36,285 INFO  [STDOUT] java.lang.ThreadDeath
	 *			at org.apache.catalina.loader.WebappClassLoader.loadClass(WebappClassLoader.java:1221)
	 */
	else if (
				contains(trimmedLine, "Error while handling scheduled job J2EE Archive Directory Agent") ||
				(
					endsWith(trimmedLine, "INFO  [STDOUT]") &&
					contains(previousLinesTrimmed[0], "Error while handling scheduled job J2EE Archive Directory Agent")
				) ||
				endsWith(trimmedLine, "java.lang.ThreadDeath")
			)
	{
		return ERROR_LINE;
	}


	/*
	 * There are 3466 SOPs in just the 2006.3 DAS module. All of the below come through as SOP's
	 * in certain scenarios. Some are hard-coded error message and others are contained in
	 * resource bundles. When printed, they often have no logging identifiers. I took all of the
	 * SOP's from Table.java, TemplateParser.java and other important classes. There were many
	 * SOP's that came from depricated code, so I just threw those out
	 *
	 * Yes I know this isn't the most elegant (messages may change, may have missed some), but
	 * it's a start!
	 *
	 * Oh and I've ran a bunch of performance test (due to all of the parsing) and there wasn't
	 * much of a memory or CPU hit at all
	 */
	else if (isSopErrorLine(trimmedLine))
	{
		return ERROR_LINE;
	}

	// more stuff that comes from SOP
	else if (
				contains(trimmedLine, "*** Checking in all assets.") ||
				contains(trimmedLine, "exporting repository:") ||
				contains(trimmedLine, "Done - Pausing, hit enter to exit")
			)
	{
		return DEBUG_LINE;
	}

	// more stuff that comes from SOP
	else if (
				startsWith(trimmedLine, "log4j:WARN") ||
				contains(trimmedLine, "There was a problem sending an invalidation event") ||
				contains(trimmedLine, "*** WARNING: Unqualified driver") ||
				startsWith(trimmedLine, "Warning:") ||
				contains(trimmedLine, "is not in the safe list") ||
				contains(trimmedLine, "Found nullable timestamp column") ||
				contains(trimmedLine, "was not found in the set of columns returned") ||
				contains(trimmedLine, "Found non-null ManyToOneMultiProperty") ||
				contains(trimmedLine, "Found unrecognized many-to-one relationship in table") ||
				contains(trimmedLine, "Found one to one definition in versioned case with both sides using a primary table") ||
				contains(trimmedLine, "Warning - table") ||
				contains(trimmedLine, "Missing a src id property in item-descriptor") ||
				contains(trimmedLine, "Missing a dst id property in item-descriptor") ||
				contains(trimmedLine, "Missing a dst multi property in item-descriptor") ||
				(contains(trimmedLine, "The property in item-descriptor") && contains(trimmedLine, "is read only.")) ||
				contains(trimmedLine, "found more than one agent status : using most recent status : older Status") ||
				contains(trimmedLine, "getConnection() should be used instead of this method") ||
				contains(trimmedLine, "Connection.close() should be used instead of this method") ||
				(contains(trimmedLine, "directory") && contains(trimmedLine, "could not be created")) ||
				(contains(trimmedLine, "Encountered") && contains(trimmedLine, "expected one of")) ||
				(contains(trimmedLine, "Class") && contains(trimmedLine, "does not have a property")) ||
				(contains(trimmedLine, "Attempt to apply the") && contains(trimmedLine, "operator to a null value")) ||
				(contains(trimmedLine, "Attempt to apply operator") && contains(trimmedLine, "to arguments of type")) ||
				(contains(trimmedLine, "Attempt to apply operator") && contains(trimmedLine, "to null value")) ||
				(contains(trimmedLine, "The function") && contains(trimmedLine, "requires") && contains(trimmedLine, "arguments but was passed")) ||

				// from /atg/nucleus/servlet/NucleusServletResources.properties
				contains(trimmedLine, "***** WARNING:  System property") ||
				contains(trimmedLine, "***** WARNING: atg_bootstrap.war may not have started first") ||
				contains(trimmedLine, "***** WARNING: Context name (context-root) of the atg_bootstrap.war was") ||
				contains(trimmedLine, "WARNING: Unsupported application server") ||
				contains(trimmedLine, "WARNING: Warning from ejbc") ||
				startsWith(trimmedLine, "Warning:") ||
				startsWith(trimmedLine, "log4j:WARN") ||
				contains(trimmedLine, "WARNING:") ||
				contains(trimmedLine, "ATG application EAR file launched in development mode")
			)
	{
		return WARNING_LINE;
	}

	else if (
				contains(trimmedLine, "Using default context-root") ||
				contains(trimmedLine, "Component browsing disabled") ||
				contains(trimmedLine, "Starting web app nucleus for application") ||
				contains(trimmedLine, "ATG-Data localconfig for default server") ||
				contains(trimmedLine, "Stopping Pointbase server...") ||
				contains(trimmedLine, "Pointbase server stopped.") ||
				startsWith(trimmedLine, "Shutdown complete") ||
				startsWith(trimmedLine, "Halting VM") ||
				contains(trimmedLine, "Configuration file read-only so engine configuration changes will not be saved") ||
				contains(trimmedLine, "ATG-Data localconfig for server") ||
				startsWith(trimmedLine, "Query: ") ||
				startsWith(trimmedLine, "INFO: ")
		)
	{
		return INFO_LINE;
	}

	// [4/5/07 18:09:14:972 CEST] 00000027 SystemOut     O /atg/portal/portletstandard/ATGContainerService PortletInvokerImpl.render() - Error while dispatching portlet. javax.portlet.PortletException
	// 2007-03-05 20:55:08,998 INFO  [STDOUT]  at org.jboss.mx.server.Invocation.dispatch(Invocation.java:80)
	else if (contains(trimmedLine, "Error while ") && previousLineType == ERROR_LINE)
	{
		return ERROR_LINE;
	}

	else if (contains(trimmedLine, "specifies an invalid item-type environnement"))
	{
		return ERROR_LINE;
	}

	else if (startsWith(trimmedLine, "LIVECONFIG=false") && endsWith(trimmedLine, "LIVECONFIG=false"))
	{
		return INFO_LINE;
	}

	else if (startsWith(trimmedLine, "LIVECONFIG=true") && endsWith(trimmedLine, "LIVECONFIG=true"))
	{
		return INFO_LINE;
	}

	/*
        this covers the period at the end of some stak traces
		at com.ibm.ws.tcp.channel.impl.WorkQueueManager$Worker.run(WorkQueueManager.java:1039)
        at com.ibm.ws.util.ThreadPool$Worker.run(ThreadPool.java(Compiled Code))
		.
	*/
	else if (previousLineType == ERROR_LINE && trimmedLine.size() == 1 && startsWith(trimmedLine, ".") && endsWith(trimmedLine, "."))
	{
		return ERROR_LINE;
	}

	// catches all websphere-specific messages. have a look at the below url:
	// http://publib.boulder.ibm.com/infocenter/wasinfo/v5r1//index.jsp?topic=/com.ibm.websphere.base.doc/info/aes/ae/ctrb_readmsglogs.html
	else if (
				isWebSphere &&
				(
					contains(trimmedLine, " E ") ||
					contains(trimmedLine, " F ") ||
					contains(trimmedLine, " R ") ||
					contains(trimmedLine, "     R 	") ||
					contains(trimmedLine, "SystemErr")
				)
			)
	{
		return ERROR_LINE;
	}
	else if (
				isWebSphere &&
				(
					contains(trimmedLine, " I ") ||
					contains(trimmedLine, " A ") ||
					contains(trimmedLine, " C ") ||
					contains(trimmedLine, " D ")
				)
			)
	{
		return INFO_LINE;
	}
	else if (
				isWebSphere &&
				contains(trimmedLine, " W ")
			)
	{
		return WARNING_LINE;
	}

	// finally, if we can't determine the line type, return info. INFO  [STDOUT] usually means
	// misconfigured ATG-specific output
	else if (contains(trimmedLine, "INFO  [STDOUT]"))
	{
		return INFO_LINE;
	}

	// if we can't find out what this line is, just return other
	return OTHER_LINE;
}

// this is called whenever ctrl + c is hit. it displays a message and resets the window text
void ctrlcCatcher(int sig)
{
	printf("\n");
	setTextColor(EXITING_COLOR_WINDOWS, console);
	printf("Ctrl + C detected\n");
	printf("Exiting now...\n");
	SetConsoleTextAttribute(console, originalwindowAttributes);
	printf("\n");
	printf("\n");
	CloseHandle(console);
	(void) signal(SIGINT, SIG_DFL);
}

// called for each line of output being processed. it trims the line, finds the line type,
// colors the line, and adds data to the arrays
void processLine(string line)
{
	if (trim(line).empty())
	{
		printf("\n");
		return;
	}
	int i=0;
	int lineType = -1;
	string trimmedLine = trim(line.c_str());
	lineType = determineLineType(line, trimmedLine, previousLineTypes, previousLines, previousLinesTrimmed);

	// after finding the line type, color the line
	switch (lineType)
	{
		case INFO_LINE:
			setTextColor(INFO_COLOR_WINDOWS, console);
			break;
		case WARNING_LINE:
			setTextColor(WARNING_COLOR_WINDOWS, console);
			break;
		case DEBUG_LINE:
			setTextColor(DEBUG_COLOR_WINDOWS, console);
			break;
		case ERROR_LINE:
			setTextColor(ERROR_COLOR_WINDOWS, console);
			break;
		case NUCLEUS_LINE:
			setTextColor(NUCLEUS_COLOR_WINDOWS, console);
			break;
		case OTHER_LINE:
			setTextColor(OTHER_COLOR_WINDOWS, console);
			break;
	}
	// printf writes out text to a console far faster than cout
	printf("%s", line.c_str()); // write the line

	cout << "\n"; // skip to next line

	// push elements up and insert the current line at position 0
	for (i = NUM_SAVED_LINES-1; i >= 0; i--)
	{
		if (i+1 <= NUM_SAVED_LINES-1)
		{
			previousLines[i+1] = previousLines[i];
		}
	}
	previousLines[0] = line;

	// push elements up and insert the current line at position 0
	for (i = NUM_SAVED_LINES-1; i >= 0; i--)
	{
		if (i+1 <= NUM_SAVED_LINES-1)
		{
			previousLinesTrimmed[i+1] = previousLinesTrimmed[i];
		}
	}
	previousLinesTrimmed[0] = trimmedLine;

	// push elements up and insert the current line at position 0
	for (i = NUM_SAVED_LINES-1; i >= 0; i--)
	{
		if (i+1 <= (NUM_SAVED_LINES-1))
		{
			previousLineTypes[i+1] = previousLineTypes[i];
		}
	}
	previousLineTypes[0] = lineType;

	//SetConsoleTextAttribute(console, originalwindowAttributes); // reset console to original color scheme

	/*
	 * this is important - after each line, set output color to yellow (other). why?
	 * when you pipe input through this application (eg. startWebLogic.cmd | ATGLogColorizer.exe),
	 * not all output is piped through. some startup scripts disobey the pipe and write directly
	 * to the console. that means that this application can't color the output properly because
	 * it doesn't know what it is! setting the output color to yellow (other) makes any output
	 * of that type show up in yellow.
	 */

	setTextColor(OTHER_COLOR_WINDOWS, console);
}

int main(int argc, char* argv[])
{
	// register the signal catcher - if ctrl + c is received, ctrlcCatcher() is called
	(void) signal(SIGINT, ctrlcCatcher);

	/*
	 * without somehow loading the floating point library, the error
	 * "runtime error R6002 - floating point not loaded" sometimes appears
	 * i know this is a hack, but hey it gets the library loaded and no more
	 * errors appear.
	 */
	float dummy;
	dummy=1.1;

    string line; // representing one line of input
	ifstream inputFile; // represents file stream when reading in log files

	// get a handle to the console. used for changing color
	HANDLE consoleStatus = (console = CreateFile(
		"CONOUT$", GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		0L, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0L));

	// INVALID_HANDLE_VALUE is if there was a problme getting the console
	if (consoleStatus == INVALID_HANDLE_VALUE)
	{
		printf("\nError: Unable to open console. Error code 1.\n");
		return 1;
	}
	GetConsoleScreenBufferInfo(console, &csbi);
	originalwindowAttributes = csbi.wAttributes; // get original window attributes (eg. colors)

	// display introduction message
	setTextColor(INFO_COLOR_WINDOWS, console);
	printf("ATG");
	setTextColor(WARNING_COLOR_WINDOWS, console);
	printf("Log");
	setTextColor(OTHER_COLOR_WINDOWS, console);
	printf("Colorizer");
	setTextColor(INTRO_COLOR_WINDOWS, console);
	printf(" v");
	printf(RELEASE_NUMBER.c_str());
	printf(". Copyleft 2007-2008 by Kelly Goetsch. http://atglogcolorizer.sourceforge.net\n");
	SetConsoleTextAttribute(console, originalwindowAttributes); // reset console to original color scheme

	// if an argument was passed in to the app. should be either -? or a fil ename
	if (argc > 1)
	{
		char* arg1 = argv[1]; // the argument

		// is this a call for help? if so, display help info
		if (contains(arg1, "?") || contains(arg1, "--?") || contains(arg1, "--help"))
		{
			setTextColor(INTRO_COLOR_WINDOWS, console);
			printf("\n");
			printf("This program is used to color-code application server output. ");
			printf("ATGLogColorizer can properly color output for JBoss, WebLogic, DAS, WebSphere, or anything using log4j. ");
			printf("\n");
			printf("Logs are colored as follows: \n");
			setTextColor(INFO_COLOR_WINDOWS, console);
			printf("Information");
			setTextColor(INTRO_COLOR_WINDOWS, console);
			printf(" - ");
			setTextColor(WARNING_COLOR_WINDOWS, console);
			printf("Warning");
			setTextColor(INTRO_COLOR_WINDOWS, console);
			printf(" - ");
			setTextColor(DEBUG_COLOR_WINDOWS, console);
			printf("Debug");
			setTextColor(INTRO_COLOR_WINDOWS, console);
			printf(" - ");
			setTextColor(ERROR_COLOR_WINDOWS, console);
			printf("Error");
			setTextColor(INTRO_COLOR_WINDOWS, console);
			printf(" - ");
			setTextColor(NUCLEUS_COLOR_WINDOWS, console);
			printf("Nucleus");
			setTextColor(INTRO_COLOR_WINDOWS, console);
			printf(" - ");
			setTextColor(OTHER_COLOR_WINDOWS, console);
			printf("Other");

			setTextColor(INTRO_COLOR_WINDOWS, console);
			printf("\n\nSample Usage: \n");
			printf("   [appserver startup script] | ATGLogColorizer.exe\n");
			printf("                   or\n");
			printf("   ATGLogColorizer.exe [path to log file]\n");
			printf("\n");
			printf("\n");

			setTextColor(WARNING_COLOR_WINDOWS, console);
			printf("Warning: This can't be used with CYGWIN on Windows\n");
			setTextColor(INTRO_COLOR_WINDOWS, console);

			printf("Bugs? Questions? Comments? Please email kgoetsch@atg.com\n");
			SetConsoleTextAttribute(console, originalwindowAttributes); // reset console to original color scheme
			return 1;
		}
		else // if the argument is not for help, assume it's a log file
		{
			setTextColor(INTRO_COLOR_WINDOWS, console);
			printf("Opening file ");
			printf(arg1);
			printf("\n");
			inputFile.open(arg1); // open file for reading
			if (inputFile.fail()) // did file fail?
			{
				// file failed, couldn't be read. write error message and abort
				setTextColor(ERROR_COLOR_WINDOWS, console);
				printf("\n");
				printf("\n");
				printf("File '");
				printf(arg1);
				printf("' couldn't be read");
				printf("\n");
				printf("\n");
				SetConsoleTextAttribute(console, originalwindowAttributes); // reset console to original color scheme
				return 1;
			}
		}
	}

	// if reading a log file, open it and process each line, one at a time
	if (argc > 1)
	{
		while(getline(inputFile,line)) // read line one at a time
		{
			processLine(stripNullChars(line)); // strip line of null characters and process it
		}
		inputFile.close(); // close file
	}
	else // otherwise, read in each line from cin
	{
		while(getline(cin,line)) // read line one at a time
		{
			processLine(stripNullChars(line)); // strip line of null characters and process it
		}
	}

	// after we're done (this only gets called when reading file logs), reset the window colors
	SetConsoleTextAttribute(console, originalwindowAttributes);
	return 1;
}
