/******************************************************************************
 * Copyright 2017 Matthieu Lefebvre
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 *       limitations under the License.
 ******************************************************************************/

#ifndef COMPAREADIOSKERNELS_ADIOS_READER_H
#define COMPAREADIOSKERNELS_ADIOS_READER_H

#include "adios_read.h"
#include <boost/mpi/communicator.hpp>
#include <vector>
#include <exception>

namespace mpi = boost::mpi;

namespace kernel_validation {

    /**
     * Basic exception to indicate when ADIOS calls go sour.
     */
    class ADIOSException : public std::exception {
        virtual const char* what() const throw()
        {
            return adios_errmsg();
        }
    } adios_exception;

    /**
     * Class providing routines to read values from a kernel file generated by specfem3d_globe.
     */
    class ADIOSReader {
        constexpr static int blocking_read = 1;
    public:
        /**
         * Constructor
         * @param filename The name of the file to be open
         * @param comm The MPI communicator to perform the read in. Probably ```world```
         * @exception adios_exception
         */
        ADIOSReader(std::string filename, mpi::communicator& comm);

        /**
         * Destructor
         * @exception adios_exception
         */
        ~ADIOSReader();

        /**
         * Read part of an array from the ADIOS file
         * @tparam T Type of the value to read. Probably ```float```
         * @param var_name Name of the array to read. Example: ```kappa_kl_crust_mantle```
         * @param rank Rank of the MPI process. Used for computing offsets.
         * @return A vector of ```T``` filled with values from the read array.
         *
         * @throw adios_exception
         */
        template<typename T>
        std::vector<T> schedule_read(const std::string& var_name, int rank);

    private:
        ADIOS_FILE* my_file;
    };

    ///////////////////////////////////////////////////////////////////////////
    /// Implementation

    ADIOSReader::ADIOSReader(std::string filename, mpi::communicator& comm)
    {
        my_file = adios_read_open_file(filename.c_str(), ADIOS_READ_METHOD_BP, comm);
        if (adios_errno) throw adios_exception;
    }

    ADIOSReader::~ADIOSReader()
    {
        adios_read_close(my_file);
        if (adios_errno) throw adios_exception;
    }

    template<typename T>
    std::vector<T> ADIOSReader::schedule_read(const std::string& var_name, int rank)
    {

        ADIOS_SELECTION* selection;
        selection = adios_selection_writeblock(rank);
        if (adios_errno) throw adios_exception;
        int local_dim_read;
        int offset_read;

        adios_schedule_read(my_file, selection, (var_name+"/local_dim").c_str(), 0, 1, &local_dim_read);
        if (adios_errno) throw adios_exception;
        adios_schedule_read(my_file, selection, (var_name+"/offset").c_str(), 0, 1, &offset_read);
        if (adios_errno) throw adios_exception;
        adios_perform_reads(my_file, blocking_read);
        if (adios_errno) throw adios_exception;

        uint64_t local_dim = local_dim_read; // up-cast
        uint64_t offset = offset_read; // up-cast
        std::vector<T> v(local_dim_read);

        selection = adios_selection_boundingbox(1, &offset, &local_dim);
        if (adios_errno) throw adios_exception;
        adios_schedule_read(my_file, selection, (var_name+"/array").c_str(), 0, 1, &v[0]);
        if (adios_errno) throw adios_exception;
        adios_perform_reads(my_file, blocking_read);
        if (adios_errno) throw adios_exception;

        //return std::move(v);
        return v;
    }

}  // namespace kernel_validation

#endif //COMPAREADIOSKERNELS_ADIOS_READER_H
