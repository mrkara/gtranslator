/*
 * (C) 2001 	Fatih Demir <kabalak@gtranslator.org>
 *
 * gtranslator is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or   
 *    (at your option) any later version.
 *    
 * gtranslator is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 *    GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "preferences.h"
#include "runtime-config.h"
#include "utils.h"

/*
 * This is the externally declared and generally used GtrRuntimeConfig.
 */
GtrRuntimeConfig *gtranslator_runtime_config=NULL;

/*
 * Create the new GtrRuntimeConfig structure with all the nice and useful
 *  values.
 */
GtrRuntimeConfig *gtranslator_runtime_config_new()
{
	GtrRuntimeConfig *config=g_new0(GtrRuntimeConfig, 1);

	/*
	 * Assign the single data fields like in the old "gtranslator_utils_"
	 *  functions from utils.c/.h.
	 */
	config->temp_filename=g_strdup_printf(
		"%s/.gtranslator/gtranslator-temp-file",
			g_get_home_dir());

	config->crash_filename=g_strdup_printf(
		"%s/.gtranslator/gtranslator-crash-file",
			g_get_home_dir());

	config->backend_filename=g_strdup_printf(
		"%s/.gtranslator/gtranslator-backend-file",
			g_get_home_dir());

	config->table_state_filename=g_strdup_printf(
		"%s/.gtranslator/etstates/gtranslator-ui-messages-table-state",
			g_get_home_dir());

	config->save_differently_filename=g_strdup_printf(
		"%s/.gtranslator/etstates/gtranslator-save-differently-file",
		g_get_home_dir());

	return config;
}

/*
 * Free the structure data we've been using till now.
 */
void gtranslator_runtime_config_free(GtrRuntimeConfig *config)
{
	if(config)
	{
		GTR_FREE(GTR_RUNTIME_CONFIG(config)->temp_filename);
		GTR_FREE(GTR_RUNTIME_CONFIG(config)->crash_filename);
		GTR_FREE(GTR_RUNTIME_CONFIG(config)->backend_filename);
		
		GTR_FREE(GTR_RUNTIME_CONFIG(config)->table_state_filename);
		GTR_FREE(GTR_RUNTIME_CONFIG(config)->save_differently_filename);
		
		GTR_FREE(config);
	}
}