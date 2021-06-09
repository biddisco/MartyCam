#include <QObject>
#include <hpx/hpx.hpp>
#include <hpx/hpx_init.hpp>
#include <hpx/include/thread_executors.hpp>
#include <hpx/future.hpp>
#include <hpx/functional/bind.hpp>
#include <hpx/program_options.hpp>

#include <cstddef>
#include <vector>

using hpx::util::high_resolution_timer;
#include <QtWidgets/QApplication>
#include "martycam.h"

#include <opencv2/opencv.hpp>

#include "capturethread.h"
#include "ConcurrentCircularBuffer.h"
#include  <boost/lockfree/queue.hpp>

#include <chrono>
#include <thread>

void qt_main(int argc, char ** argv)
{
    //  QApplication app(nCmdShow, NULL);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("MartyCam");
    QCoreApplication::setOrganizationDomain("MartyCam");
    QCoreApplication::setApplicationName("MartyCam");
    //
    // App icon
    //
    QIcon appIcon(":/images/Marty.png");
    app.setWindowIcon(appIcon);
    //
    hpx::execution::parallel_executor defaultExecutor(&hpx::resource::get_thread_pool("default"));
    hpx::execution::parallel_executor blockingExecutor(&hpx::resource::get_thread_pool("blocking"));
    std::cout << "[hpx_main] Created default and " << "blocking"
              << " pool_executors" << std::endl;
    MartyCam *tracker = new MartyCam(defaultExecutor, blockingExecutor);
    tracker->show();
    app.exec();
}

void init_resource_partitioner_handler(hpx::resource::partitioner& rp, hpx::program_options::variables_map const&)
{
    // Create the resource partitioner
    std::cout << "[main] obtained reference to the resource_partitioner" << std::endl;

    rp.create_thread_pool("default",
                          hpx::resource::scheduling_policy::local_priority_fifo);
    std::cout << "[main] " << "thread_pool default created" << std::endl;

    // Create a thread pool using the default scheduler provided by HPX
    rp.create_thread_pool("blocking",
                          hpx::resource::scheduling_policy::local_priority_fifo);

    std::cout << "[main] " << "thread_pool blocking created" << std::endl;

    int blocking_tp_num_threads = 3; // vm["blocking_tp_num_threads"].as<int>();

    // add PUs to opencv pool
    int count = 0;
    for (const hpx::resource::numa_domain& d : rp.numa_domains())
    {
        for (const hpx::resource::core& c : d.cores())
        {
            for (const hpx::resource::pu& p : c.pus())
            {
                if (count < blocking_tp_num_threads)
                {
                    std::cout << "[main] Added pu " << count++ << " to " <<
                              "blocking" << " thread pool" << std::endl;
                    rp.add_resource(p, "blocking");
                }
            }
        }
    }

    std::cout << "[main] resources added to thread_pools" << std::endl;

}

int hpx_main(int argc, char ** argv)
{
    // Get a reference to one of the main threads
    hpx::execution::parallel_executor scheduler(&hpx::resource::get_thread_pool("default"));

    // run an async function on the main thread to start the Qt application
    // Store a pointer to the runtime here.
    auto rt = hpx::get_runtime_ptr();
    std::thread qt_thread([&](){
        hpx::error_code ec(hpx::lightweight);
        hpx::register_thread(rt, "Qt", ec);
        //
        qt_main(argc, argv);
        //
        hpx::unregister_thread(rt);
    });
    qt_thread.join();

    return hpx::finalize();
}

int main(int argc, char **argv)
{
    namespace po = hpx::program_options;
    po::options_description desc_cmdline("Options");
    desc_cmdline.add_options()
            ("blocking_tp_num_threads,m",
             po::value<int>()->default_value(1),
             "Number of threads to assign to blocking pool");

    // HPX uses a boost program options variable map, but we need it before
    // hpx-main, so we will create another one here and throw it away after use
    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv).allow_unregistered()
                          .options(desc_cmdline).run(), vm);
    }
    catch(po::error& e) {
        std::cerr << "ERROR: " << e.what() << "\n\n";
        std::cerr << desc_cmdline << std::endl;
        return -1;
    }

    // Setup the init parameters
    hpx::init_params init_args;
    init_args.desc_cmdline = desc_cmdline;

    // Set the callback to init the thread_pools
    init_args.rp_callback = &init_resource_partitioner_handler;
    return hpx::init(argc, argv, init_args);
}
