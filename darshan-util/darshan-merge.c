#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <glob.h>

#include "uthash-1.9.2/src/uthash.h"

#include "darshan-logutils.h"

#define DEF_MOD_BUF_SIZE 1024 /* 1 KiB is enough for all current mod records ... */

/* TODO: set job end timestamp? */

struct darshan_shared_record_ref
{
    darshan_record_id id;
    int ref_cnt;
    char agg_rec[DEF_MOD_BUF_SIZE];
    UT_hash_handle hlink;
};

void usage(char *exename)
{
    fprintf(stderr, "Usage: %s --output <output_path> [options] <input-logs>\n", exename);
    fprintf(stderr, "This utility merges multiple Darshan log files into a single output log file.\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "\t--output\t(REQUIRED) Full path of the output darshan log file.\n");
    fprintf(stderr, "\t--shared-redux\tReduce globally shared records into a single record.\n");

    exit(1);
}

void parse_args(int argc, char **argv, char ***infile_list, int *n_files,
    char **outlog_path, int *shared_redux)
{
    int index;
    static struct option long_opts[] =
    {
        {"shared-redux", no_argument, NULL, 's'},
        {"output", required_argument, NULL, 'o'},
        {0, 0, 0, 0}
    };

    *shared_redux = 0;
    *outlog_path = NULL;

    while(1)
    {
        int c = getopt_long(argc, argv, "", long_opts, &index);

        if(c == -1) break;

        switch(c)
        {
            case 's':
                *shared_redux = 1;
                break;
            case 'o':
                *outlog_path = optarg;
                break;
            case '?':
            default:
                usage(argv[0]);
                break;
        }
    }

    if(*outlog_path == NULL)
    {
        usage(argv[0]);
    }

    *infile_list = &argv[optind];
    *n_files = argc - optind;

    return;
}

int build_mod_shared_rec_hash(char **infile_list, int n_infiles,
    darshan_module_id mod_id, int nprocs, char *mod_buf,
    struct darshan_shared_record_ref **shared_rec_hash)
{
    darshan_fd in_fd;
    struct darshan_base_record *base_rec;
    struct darshan_shared_record_ref *ref, *tmp;
    int init = 0;
    int ret;
    int i;

    /* loop over each input log file */
    for(i = 0; i < n_infiles; i++)
    {
        in_fd = darshan_log_open(infile_list[i]);
        if(in_fd == NULL)
        {
            fprintf(stderr,
                "Error: unable to open input Darshan log file %s.\n",
                infile_list[i]);
            return(-1);
        }

        while((ret = mod_logutils[mod_id]->log_get_record(in_fd, mod_buf)) == 1)
        {
            base_rec = (struct darshan_base_record *)mod_buf;

            /* initialize the hash with the first rank's records */
            if(!init)
            {
                struct darshan_base_record *agg_base;

                /* create a new ref and add to the hash */
                ref = malloc(sizeof(*ref));
                if(!ref)
                {
                    darshan_log_close(in_fd);
                    return(-1);
                }
                memset(ref, 0, sizeof(*ref));

                /* initialize the aggregate record with this rank's record */
                agg_base = (struct darshan_base_record *)ref->agg_rec;
                agg_base->id = base_rec->id;
                agg_base->rank = -1;
                mod_logutils[mod_id]->log_agg_records(mod_buf, ref->agg_rec, 1);

                ref->id = base_rec->id;
                ref->ref_cnt = 1;
                HASH_ADD(hlink, *shared_rec_hash, id, sizeof(darshan_record_id), ref);
                init = 1;
            }
            else
            {
                /* search for this record in shared record hash */
                HASH_FIND(hlink, *shared_rec_hash, &(base_rec->id),
                    sizeof(darshan_record_id), ref);
                if(ref)
                {
                    /* if found, aggregate this rank's record into the shared record */
                    mod_logutils[mod_id]->log_agg_records(mod_buf, ref->agg_rec, 0);
                    ref->ref_cnt++;
                }
            }
        }
        if(ret < 0)
        {
            fprintf(stderr,
                "Error: unable to read %s module record from input log file %s.\n",
                darshan_module_names[mod_id], infile_list[i]);
            darshan_log_close(in_fd);
            return(-1);
        }

        darshan_log_close(in_fd);
    }

    /* prune any non-shared records from the hash one last time */
    HASH_ITER(hlink, *shared_rec_hash, ref, tmp)
    {
        if(ref->ref_cnt != nprocs)
        {
            HASH_DELETE(hlink, *shared_rec_hash, ref);
            free(ref);
        }
    }

    return(0);
}

int main(int argc, char *argv[])
{
    char **infile_list;
    int n_infiles;
    int shared_redux;
    char *outlog_path;
    darshan_fd in_fd, merge_fd;
    struct darshan_job in_job, merge_job;
    char merge_exe[DARSHAN_EXE_LEN+1] = {0};
    char **merge_mnt_pts;
    char **merge_fs_types;
    int merge_mnt_count = 0;
    struct darshan_record_ref *in_hash = NULL;
    struct darshan_record_ref *merge_hash = NULL;
    struct darshan_record_ref *ref, *tmp, *found;
    struct darshan_shared_record_ref *shared_rec_hash = NULL;
    struct darshan_shared_record_ref *sref, *stmp;
    struct darshan_base_record *base_rec;
    char mod_buf[DEF_MOD_BUF_SIZE];
    int i, j;
    int ret;

    /* grab command line arguments */
    parse_args(argc, argv, &infile_list, &n_infiles, &outlog_path, &shared_redux);

    memset(&merge_job, 0, sizeof(struct darshan_job));

    /* first pass at merging together logs:
     *      - compose output job-level metadata structure (including exe & mount data)
     *      - compose output record_id->file_name mapping 
     */
    for(i = 0; i < n_infiles; i++)
    {
        memset(&in_job, 0, sizeof(struct darshan_job));

        in_fd = darshan_log_open(infile_list[i]);
        if(in_fd == NULL)
        {
            fprintf(stderr,
                "Error: unable to open input Darshan log file %s.\n",
                infile_list[i]);
            return(-1);
        }

        /* read job-level metadata from the input file */
        ret = darshan_log_getjob(in_fd, &in_job);
        if(ret < 0)
        {
            fprintf(stderr,
                "Error: unable to read job data from input Darshan log file %s.\n",
                infile_list[i]);
            darshan_log_close(in_fd);
            return(-1);
        }

        /* if the input darshan log has metadata set indicating the darshan
         * shutdown procedure was called on the log, then we error out. if the
         * shutdown procedure was started, then it's possible the log has
         * incomplete or corrupt data, so we just throw out the data for now.
         */
        if(strstr(in_job.metadata, "darshan_shutdown=yes"))
        {
            fprintf(stderr,
                "Error: potentially corrupt data found in input log file %s.\n",
                infile_list[i]);
            darshan_log_close(in_fd);
            return(-1);
        }

        if(i == 0)
        {
            /* get job data, exe, & mounts directly from the first input log */
            memcpy(&merge_job, &in_job, sizeof(struct darshan_job));

            ret = darshan_log_getexe(in_fd, merge_exe);
            if(ret < 0)
            {
                fprintf(stderr,
                    "Error: unable to read exe string from input Darshan log file %s.\n",
                    infile_list[i]);
                darshan_log_close(in_fd);
                return(-1);
            }

            ret = darshan_log_getmounts(in_fd, &merge_mnt_pts,
                &merge_fs_types, &merge_mnt_count);
            if(ret < 0)
            {
                fprintf(stderr,
                    "Error: unable to read mount info from input Darshan log file %s.\n",
                    infile_list[i]);
                darshan_log_close(in_fd);
                return(-1);
            }
        }
        else
        {
            /* potentially update job timestamps using remaining logs */
            if(in_job.start_time < merge_job.start_time)
                merge_job.start_time = in_job.start_time;
            if(in_job.end_time > merge_job.end_time)
                merge_job.end_time = in_job.end_time;
        }

        /* read the hash of ids->names for the input log */
        ret = darshan_log_gethash(in_fd, &in_hash);
        if(ret < 0)
        {
            fprintf(stderr,
                "Error: unable to read job data from input Darshan log file %s.\n",
                infile_list[i]);
            darshan_log_close(in_fd);
            return(-1);
        }

        /* iterate the input hash, copying over record_id->file_name mappings
         * that have not already been copied to the output hash
         */
        HASH_ITER(hlink, in_hash, ref, tmp)
        {
            HASH_FIND(hlink, merge_hash, &(ref->id), sizeof(darshan_record_id), found);
            if(!found)
            {
                HASH_ADD(hlink, merge_hash, id, sizeof(darshan_record_id), ref);
            }
            else if(strcmp(ref->name, found->name))
            {
                fprintf(stderr,
                    "Error: invalid Darshan record table entry.\n");
                darshan_log_close(in_fd);
                return(-1);
            }
        }

        darshan_log_close(in_fd);
    }

    /* create the output "merged" log */
    merge_fd = darshan_log_create(outlog_path, DARSHAN_ZLIB_COMP, 1);
    if(merge_fd == NULL)
    {
        fprintf(stderr, "Error: unable to create output darshan log.\n");
        return(-1);
    }

    /* write the darshan job info, exe string, and mount data to output file */
    ret = darshan_log_putjob(merge_fd, &merge_job);
    if(ret < 0)
    {
        fprintf(stderr, "Error: unable to write job data to output darshan log.\n");
        darshan_log_close(merge_fd);
        unlink(outlog_path);
        return(-1);
    }

    ret = darshan_log_putexe(merge_fd, merge_exe);
    if(ret < 0)
    {
        fprintf(stderr, "Error: unable to write exe string to output darshan log.\n");
        darshan_log_close(merge_fd);
        unlink(outlog_path);
        return(-1);
    }

    ret = darshan_log_putmounts(merge_fd, merge_mnt_pts, merge_fs_types, merge_mnt_count);
    if(ret < 0)
    {
        fprintf(stderr, "Error: unable to write mount data to output darshan log.\n");
        darshan_log_close(merge_fd);
        unlink(outlog_path);
        return(-1);
    }

    /* write the merged table of records to output file */
    ret = darshan_log_puthash(merge_fd, merge_hash);
    if(ret < 0)
    {
        fprintf(stderr, "Error: unable to write record table to output darshan log.\n");
        darshan_log_close(merge_fd);
        unlink(outlog_path);
        return(-1);
    }

    /* iterate over active darshan modules and gather module data to write
     * to the merged output log
     */
    for(i = 0; i < DARSHAN_MAX_MODS; i++)
    {
        if(!mod_logutils[i]) continue;

        if(shared_redux)
        {
            /* build the hash of records shared globally by this module */
            ret = build_mod_shared_rec_hash(infile_list, n_infiles, i,
                merge_job.nprocs, mod_buf, &shared_rec_hash);
            if(ret < 0)
            {
                fprintf(stderr,
                    "Error: unable to build list of %s module's shared records.\n",
                    darshan_module_names[i]);
                darshan_log_close(merge_fd);
                unlink(outlog_path);
                return(-1);
            }

        }

        for(j = 0; j < n_infiles; j++)
        {
            in_fd = darshan_log_open(infile_list[j]);
            if(in_fd == NULL)
            {
                fprintf(stderr,
                    "Error: unable to open input Darshan log file %s.\n",
                    infile_list[j]);
                darshan_log_close(merge_fd);
                unlink(outlog_path);
                return(-1);
            }

            if(j == 0 && shared_rec_hash)
            {
                /* write out the shared records first */
                HASH_ITER(hlink, shared_rec_hash, sref, stmp)
                {
                    ret = mod_logutils[i]->log_put_record(merge_fd, sref->agg_rec, in_fd->mod_ver[i]);
                    if(ret < 0)
                    {
                        fprintf(stderr,
                            "Error: unable to write %s module record to output darshan log.\n",
                            darshan_module_names[i]);
                        darshan_log_close(in_fd);
                        darshan_log_close(merge_fd);
                        unlink(outlog_path);
                        return(-1);
                    }
                }
            }

            /* loop over module records and write them to output file */
            while((ret = mod_logutils[i]->log_get_record(in_fd, mod_buf)) == 1)
            {
                base_rec = (struct darshan_base_record *)mod_buf;

                HASH_FIND(hlink, shared_rec_hash, &(base_rec->id), sizeof(darshan_record_id), sref);
                if(sref)
                    continue; /* skip shared records */

                ret = mod_logutils[i]->log_put_record(merge_fd, mod_buf, in_fd->mod_ver[i]);
                if(ret < 0)
                {
                    fprintf(stderr,
                        "Error: unable to write %s module record to output log file %s.\n",
                        darshan_module_names[i], infile_list[j]);
                    darshan_log_close(in_fd);
                    darshan_log_close(merge_fd);
                    unlink(outlog_path);
                    return(-1);
                }
            }
            if(ret < 0)
            {
                fprintf(stderr,
                    "Error: unable to read %s module record from input log file %s.\n",
                    darshan_module_names[i], infile_list[j]);
                darshan_log_close(in_fd);
                darshan_log_close(merge_fd);
                unlink(outlog_path);
                return(-1);
            }

            darshan_log_close(in_fd);
        }

        /* clear the shared record hash for the next module */
        if(shared_redux)
        {
            HASH_ITER(hlink, shared_rec_hash, sref, stmp)
            {
                HASH_DELETE(hlink, shared_rec_hash, sref);
                free(sref);
            }
        }
    }

    darshan_log_close(merge_fd);

    return(0);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */
